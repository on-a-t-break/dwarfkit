#include "DkUnrealProviders.h"

#include "DkUnrealAsync.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WebSocketsModule.h"

namespace
{

// the same bounds the curl providers apply
constexpr float HttpTimeoutSeconds = 30.0f;
constexpr uint32 ConnectTimeoutMs = 30000;
constexpr int64 MaxResponseBytes = 64 * 1024 * 1024;
constexpr int64 MaxMessageBytes = 16 * 1024 * 1024;
constexpr int64 MaxQueuedBytes = 64 * 1024 * 1024;

FString ToFString(std::string_view Value)
{
    const std::string Utf8(Value);
    return FString(UTF8_TO_TCHAR(Utf8.c_str()));
}

std::string ToStdString(const FString& Value)
{
    return std::string(TCHAR_TO_UTF8(*Value));
}

dwarfkit::Error TransportError(const char* Message, const char* Code)
{
    dwarfkit::Error Error;
    Error.kind = dwarfkit::ErrorKind::Transport;
    Error.message = Message;
    Error.details = dwarfkit::json{{"code", Code}};
    return Error;
}

dwarfkit::Error Cancelled()
{
    return dwarfkit::Error{dwarfkit::ErrorKind::Canceled, "Cancelled", 0, {}};
}

}  // namespace

// ---- FDkUnrealFetchProvider ------------------------------------------------

dwarfkit::Result<dwarfkit::FetchResponse> FDkUnrealFetchProvider::fetch(
    const dwarfkit::FetchRequest& Request)
{
    if (!ensureMsgf(!IsInGameThread(),
                    TEXT("Dwarfkit: kit calls block and must not run on the game thread")))
    {
        return dwarfkit::err(TransportError("Kit called on the game thread", "E_NETWORK"));
    }

    // shared with the completion delegate, which may fire after a timeout
    // has already returned control to the worker
    struct FHttpState
    {
        FEventRef Done{EEventMode::ManualReset};
        dwarfkit::FetchResponse Response;
        std::atomic<bool> bSucceeded{false};
        std::atomic<bool> bTooLarge{false};
    };
    TSharedRef<FHttpState, ESPMode::ThreadSafe> State =
        MakeShared<FHttpState, ESPMode::ThreadSafe>();

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
    Http->SetURL(ToFString(Request.url));
    Http->SetVerb(ToFString(Request.method));
    if (!Request.body.empty())
    {
        Http->SetContentAsString(ToFString(Request.body));
    }
    for (const auto& Header : Request.headers)
    {
        Http->SetHeader(ToFString(Header.first), ToFString(Header.second));
    }
    Http->SetTimeout(HttpTimeoutSeconds);
    // complete on the HTTP thread: the worker's wait then never depends on
    // the game thread ticking, and cannot deadlock a game thread that waits
    // on the worker
    Http->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

    Http->OnProcessRequestComplete().BindLambda(
        [State](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bConnected)
        {
            if (bConnected && HttpResponse.IsValid())
            {
                // raw bytes rather than a TCHAR round trip: a response body is
                // UTF-8 JSON and must reach the parser unchanged
                const TArray<uint8>& Content = HttpResponse->GetContent();
                if (Content.Num() > MaxResponseBytes)
                {
                    State->bTooLarge = true;
                }
                else
                {
                    State->Response.status = HttpResponse->GetResponseCode();
                    State->Response.body.assign(
                        reinterpret_cast<const char*>(Content.GetData()),
                        static_cast<size_t>(Content.Num()));
                    for (const FString& Header : HttpResponse->GetAllHeaders())
                    {
                        FString Key;
                        FString Value;
                        if (Header.Split(TEXT(": "), &Key, &Value))
                        {
                            State->Response.headers.emplace_back(ToStdString(Key),
                                                                 ToStdString(Value));
                        }
                    }
                    State->bSucceeded = true;
                }
            }
            State->Done->Trigger();
        });
    Http->ProcessRequest();

    // the request's own timeout normally fires first; the outer bound is a
    // backstop, and cancellation ends the wait immediately
    if (!DkUnreal::WaitEvent(State->Done, Token,
                             static_cast<uint32>(HttpTimeoutSeconds * 1000.0f) + 5000))
    {
        Http->CancelRequest();
        if (Token.cancelled())
        {
            return dwarfkit::err(Cancelled());
        }
        return dwarfkit::err(TransportError("HTTP request timed out", "E_TIMEOUT"));
    }
    if (State->bTooLarge)
    {
        return dwarfkit::err(TransportError("HTTP response too large", "E_NETWORK"));
    }
    if (!State->bSucceeded)
    {
        return dwarfkit::err(TransportError("HTTP request failed", "E_NETWORK"));
    }
    return MoveTemp(State->Response);
}

// ---- FDkUnrealWebSocketProvider --------------------------------------------

FDkUnrealWebSocketProvider::~FDkUnrealWebSocketProvider()
{
    close();
}

dwarfkit::Result<void> FDkUnrealWebSocketProvider::connect(std::string_view Url)
{
    close();
    TSharedRef<FState, ESPMode::ThreadSafe> NewState = MakeShared<FState, ESPMode::ThreadSafe>();
    State = NewState;
    const FString UrlText = ToFString(Url);

    // the socket lives on the game thread: created there, its delegates fire
    // there, and they capture only the shared state
    const bool bRan = DkUnreal::RunOnGameThreadAndWait(
        [NewState, UrlText]()
        {
            TSharedPtr<IWebSocket> Socket = FWebSocketsModule::Get().CreateWebSocket(UrlText);
            Socket->OnConnected().AddLambda(
                [NewState]()
                {
                    NewState->bConnected = true;
                    NewState->ConnectEvent->Trigger();
                });
            Socket->OnConnectionError().AddLambda(
                [NewState](const FString&)
                {
                    NewState->bErrored = true;
                    NewState->ConnectEvent->Trigger();
                    NewState->MessageEvent->Trigger();
                });
            Socket->OnClosed().AddLambda(
                [NewState](int32, const FString&, bool)
                {
                    NewState->bClosed = true;
                    NewState->ConnectEvent->Trigger();
                    NewState->MessageEvent->Trigger();
                });
            // raw frames only: the text delegate would queue every text
            // message a second time and round-trip it through TCHAR. Frames
            // arrive in order with BytesRemaining counting down to the end of
            // the message, so accumulate until zero.
            Socket->OnRawMessage().AddLambda(
                [NewState](const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
                {
                    FScopeLock Lock(&NewState->Lock);
                    const int64 MessageBytes = static_cast<int64>(NewState->Partial.Num()) +
                                               static_cast<int64>(Size) +
                                               static_cast<int64>(BytesRemaining);
                    if (MessageBytes > MaxMessageBytes || NewState->QueuedBytes > MaxQueuedBytes)
                    {
                        NewState->bErrored = true;
                        NewState->MessageEvent->Trigger();
                        return;
                    }
                    NewState->Partial.Append(static_cast<const uint8*>(Data),
                                             static_cast<int32>(Size));
                    if (BytesRemaining == 0)
                    {
                        NewState->QueuedBytes += NewState->Partial.Num();
                        NewState->Messages.Emplace(MoveTemp(NewState->Partial));
                        NewState->Partial.Reset();
                        NewState->MessageEvent->Trigger();
                    }
                });
            NewState->Socket = Socket;
            Socket->Connect();
        },
        Token, ConnectTimeoutMs);
    if (!bRan)
    {
        State.Reset();
        return dwarfkit::err(Token.cancelled() ? Cancelled()
                                               : TransportError("Game thread unavailable",
                                                                "E_NETWORK"));
    }

    DkUnreal::WaitEvent(NewState->ConnectEvent, Token, ConnectTimeoutMs);
    if (!NewState->bConnected)
    {
        const bool bWasCancelled = Token.cancelled();
        close();
        if (bWasCancelled)
        {
            return dwarfkit::err(Cancelled());
        }
        return dwarfkit::err(NewState->bErrored || NewState->bClosed
                                 ? TransportError("WebSocket connect failed", "E_NETWORK")
                                 : TransportError("WebSocket connect timed out", "E_TIMEOUT"));
    }
    return {};
}

dwarfkit::Result<dwarfkit::Bytes> FDkUnrealWebSocketProvider::receive(
    std::chrono::milliseconds Timeout, dwarfkit::CancelToken InToken)
{
    TSharedPtr<FState, ESPMode::ThreadSafe> Current = State;
    if (!Current.IsValid())
    {
        return dwarfkit::err(TransportError("WebSocket is not connected", "E_NETWORK"));
    }
    const double Deadline =
        FPlatformTime::Seconds() + static_cast<double>(Timeout.count()) / 1000.0;
    for (;;)
    {
        {
            FScopeLock Lock(&Current->Lock);
            if (Current->Messages.Num() > 0)
            {
                TArray<uint8> Message = MoveTemp(Current->Messages[0]);
                Current->Messages.RemoveAt(0);
                Current->QueuedBytes -= Message.Num();
                return dwarfkit::Bytes(std::vector<uint8_t>(
                    Message.GetData(), Message.GetData() + Message.Num()));
            }
            if (Current->bErrored || Current->bClosed)
            {
                return dwarfkit::err(TransportError("WebSocket closed", "E_NETWORK"));
            }
        }
        if (InToken.cancelled() || Token.cancelled())
        {
            return dwarfkit::err(Cancelled());
        }
        const double Remaining = Deadline - FPlatformTime::Seconds();
        if (Remaining <= 0)
        {
            return dwarfkit::err(TransportError("WebSocket receive timed out", "E_TIMEOUT"));
        }
        // wake regularly to observe cancellation
        const uint32 SliceMs =
            static_cast<uint32>(FMath::Clamp(Remaining * 1000.0, 1.0, 250.0));
        Current->MessageEvent->Wait(SliceMs);
    }
}

dwarfkit::Result<void> FDkUnrealWebSocketProvider::send(std::span<const uint8_t> Data)
{
    TSharedPtr<FState, ESPMode::ThreadSafe> Current = State;
    if (!Current.IsValid() || !Current->Socket.IsValid() || !Current->bConnected ||
        Current->bClosed)
    {
        return dwarfkit::err(TransportError("WebSocket is not connected", "E_NETWORK"));
    }
    Current->Socket->Send(Data.data(), Data.size(), /*bIsBinary*/ true);
    return {};
}

void FDkUnrealWebSocketProvider::close()
{
    TSharedPtr<FState, ESPMode::ThreadSafe> Current = State;
    State.Reset();
    if (!Current.IsValid())
    {
        return;
    }
    Current->bClosed = true;
    Current->MessageEvent->Trigger();
    Current->ConnectEvent->Trigger();
    TSharedPtr<IWebSocket> Socket = Current->Socket;
    Current->Socket.Reset();
    if (Socket.IsValid())
    {
        // closed on the game thread like it was created; unbinding the
        // delegates breaks the socket -> delegates -> state -> socket cycle.
        // Not waited on: nothing in the caller depends on it.
        AsyncTask(ENamedThreads::GameThread,
                  [Socket]()
                  {
                      Socket->OnConnected().Clear();
                      Socket->OnConnectionError().Clear();
                      Socket->OnClosed().Clear();
                      Socket->OnRawMessage().Clear();
                      if (Socket->IsConnected())
                      {
                          Socket->Close();
                      }
                  });
    }
}

// ---- FDkUnrealStorage ------------------------------------------------------

FString FDkUnrealStorage::PathFor(std::string_view Key)
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Dwarfkit"),
                           ToFString(Key) + TEXT(".txt"));
}

dwarfkit::Result<void> FDkUnrealStorage::write(std::string_view Key, std::string_view Data)
{
    if (!FFileHelper::SaveStringToFile(ToFString(Data), *PathFor(Key)))
    {
        return dwarfkit::err(dwarfkit::ErrorKind::Storage, "Failed to write session storage");
    }
    return {};
}

dwarfkit::Result<std::optional<std::string>> FDkUnrealStorage::read(std::string_view Key)
{
    FString Data;
    if (!FPaths::FileExists(PathFor(Key)))
    {
        return std::optional<std::string>();
    }
    if (!FFileHelper::LoadFileToString(Data, *PathFor(Key)))
    {
        return dwarfkit::err(dwarfkit::ErrorKind::Storage, "Failed to read session storage");
    }
    return std::optional(ToStdString(Data));
}

dwarfkit::Result<void> FDkUnrealStorage::remove(std::string_view Key)
{
    IFileManager::Get().Delete(*PathFor(Key), /*RequireExists*/ false);
    return {};
}
