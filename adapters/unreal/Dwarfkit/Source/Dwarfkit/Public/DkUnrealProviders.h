// Dwarfkit transport/storage providers over Unreal subsystems (BLUEPRINT.md
// 8.1). Kits must run on a worker thread; these providers block that worker,
// never the game thread. Every wait has a bound and honours the kit's
// CancelToken, and every buffer that grows from network data has a cap
// (mirroring the curl providers).
#pragma once

#include <atomic>

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <dwarfkit/core/cancel.hpp>
#include <dwarfkit/session.hpp>
#include <dwarfkit/transport/fetch_provider.hpp>
#include <dwarfkit/transport/websocket_provider.hpp>
THIRD_PARTY_INCLUDES_END

class IWebSocket;

// FHttpModule request issued from the worker. Completion is delivered on the
// HTTP thread rather than the game thread, so the worker's wait never
// depends on the game thread ticking.
class FDkUnrealFetchProvider final : public dwarfkit::FetchProvider
{
public:
    explicit FDkUnrealFetchProvider(dwarfkit::CancelToken InToken) : Token(MoveTemp(InToken)) {}

    dwarfkit::Result<dwarfkit::FetchResponse> fetch(
        const dwarfkit::FetchRequest& Request) override;

private:
    dwarfkit::CancelToken Token;
};

// FWebSocketsModule socket. It is created, connected and closed on the game
// thread, where its delegates fire; messages are reassembled from raw frames
// into a queue behind a critical section, and receive() blocks the worker
// with the requested timeout and cancel token.
class FDkUnrealWebSocketProvider final : public dwarfkit::WebSocketProvider
{
public:
    explicit FDkUnrealWebSocketProvider(dwarfkit::CancelToken InToken)
        : Token(MoveTemp(InToken))
    {
    }
    ~FDkUnrealWebSocketProvider() override;

    dwarfkit::Result<void> connect(std::string_view Url) override;
    dwarfkit::Result<dwarfkit::Bytes> receive(std::chrono::milliseconds Timeout,
                                              dwarfkit::CancelToken InToken) override;
    dwarfkit::Result<void> send(std::span<const uint8_t> Data) override;
    void close() override;

    // Shared with the socket delegates, so it outlives this object and a
    // late delegate never touches freed memory.
    struct FState
    {
        TSharedPtr<IWebSocket> Socket;
        FCriticalSection Lock;
        TArray<TArray<uint8>> Messages;
        TArray<uint8> Partial;
        int64 QueuedBytes = 0;
        FEventRef ConnectEvent{EEventMode::ManualReset};
        FEventRef MessageEvent{EEventMode::AutoReset};
        std::atomic<bool> bConnected{false};
        std::atomic<bool> bErrored{false};
        std::atomic<bool> bClosed{false};
    };

private:
    TSharedPtr<FState, ESPMode::ThreadSafe> State;
    dwarfkit::CancelToken Token;
};

// Session storage under <ProjectSaved>/Dwarfkit/ through the engine's file
// layer, so it works wherever the engine does.
class FDkUnrealStorage final : public dwarfkit::SessionStorage
{
public:
    dwarfkit::Result<void> write(std::string_view Key, std::string_view Data) override;
    dwarfkit::Result<std::optional<std::string>> read(std::string_view Key) override;
    dwarfkit::Result<void> remove(std::string_view Key) override;

private:
    static FString PathFor(std::string_view Key);
};
