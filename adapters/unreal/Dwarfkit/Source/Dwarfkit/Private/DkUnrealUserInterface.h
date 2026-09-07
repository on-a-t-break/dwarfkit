// dwarfkit::UserInterface bridged to UDwarfkitUI: calls arrive on the kit
// worker, are dispatched to the game thread and awaited with a bound and the
// cancel token. The UI is looked up through the subsystem at call time, so
// SetUI takes effect whenever it is called.
#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include <dwarfkit/session.hpp>
THIRD_PARTY_INCLUDES_END

#include "DkUnrealAsync.h"
#include "DwarfkitSubsystem.h"
#include "DwarfkitUI.h"

class FDkUnrealUserInterface final : public dwarfkit::AbstractUserInterface
{
public:
    explicit FDkUnrealUserInterface(TWeakObjectPtr<UDwarfkitSubsystem> InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    dwarfkit::Result<dwarfkit::UserInterfaceLoginResponse> login(
        dwarfkit::LoginContext& Context) override
    {
        // chain and permission come from the async node arguments; the first
        // wallet plugin is used
        dwarfkit::UserInterfaceLoginResponse Response;
        if (Context.chain)
        {
            Response.chainId = Context.chain->id;
        }
        Response.permissionLevel = Context.permissionLevel;
        Response.walletPluginIndex = 0;
        return Response;
    }

    dwarfkit::Result<void> onError(const dwarfkit::Error& Error) override
    {
        RunOnGameThread([Message = FString(UTF8_TO_TCHAR(Error.message.c_str()))](
                            UDwarfkitUI* Widget) { Widget->OnErrorMessage(Message); },
                        dwarfkit::CancelToken());
        return {};
    }

    dwarfkit::Result<dwarfkit::UserInterfaceAccountCreationResponse> onAccountCreate(
        dwarfkit::CreateAccountContext&) override
    {
        return dwarfkit::err(dwarfkit::ErrorKind::Unsupported,
                             "Account creation UI is not implemented");
    }
    dwarfkit::Result<void> onAccountCreateComplete() override { return {}; }
    dwarfkit::Result<void> onLogin() override { return {}; }
    dwarfkit::Result<void> onLoginComplete() override { return {}; }
    dwarfkit::Result<void> onTransact() override { return {}; }
    dwarfkit::Result<void> onTransactComplete() override { return {}; }
    dwarfkit::Result<void> onSign() override { return {}; }
    dwarfkit::Result<void> onSignComplete() override { return {}; }
    dwarfkit::Result<void> onBroadcast() override { return {}; }
    dwarfkit::Result<void> onBroadcastComplete() override { return {}; }

    dwarfkit::Result<dwarfkit::PromptResponse> prompt(const dwarfkit::PromptArgs& Args,
                                                      dwarfkit::CancelToken Token) override
    {
        if (Token.cancelled())
        {
            return dwarfkit::err(dwarfkit::ErrorKind::Canceled, "Prompt cancelled");
        }
        TArray<FDkPromptElement> Elements;
        for (const auto& Element : Args.elements)
        {
            FDkPromptElement Converted;
            switch (Element.type)
            {
                case dwarfkit::PromptElementType::qr: Converted.Type = TEXT("qr"); break;
                case dwarfkit::PromptElementType::link: Converted.Type = TEXT("link"); break;
                case dwarfkit::PromptElementType::button: Converted.Type = TEXT("button"); break;
                case dwarfkit::PromptElementType::countdown:
                    Converted.Type = TEXT("countdown");
                    break;
                case dwarfkit::PromptElementType::textarea:
                    Converted.Type = TEXT("textarea");
                    break;
                default: Converted.Type = TEXT("other"); break;
            }
            if (Element.label)
            {
                Converted.Label = UTF8_TO_TCHAR(Element.label->c_str());
            }
            Converted.DataJson = UTF8_TO_TCHAR(DataText(Element.data).c_str());
            Elements.Add(MoveTemp(Converted));
        }
        RunOnGameThread(
            [Title = FString(UTF8_TO_TCHAR(Args.title.c_str())),
             Body = FString(UTF8_TO_TCHAR(Args.body.value_or("").c_str())),
             Elements = MoveTemp(Elements)](UDwarfkitUI* Widget)
            { Widget->OnPrompt(Title, Body, Elements); },
            Token);
        return dwarfkit::PromptResponse{};
    }

    void status(const std::string& Message) override
    {
        RunOnGameThread([Text = FString(UTF8_TO_TCHAR(Message.c_str()))](UDwarfkitUI* Widget)
                        { Widget->OnStatus(Text); },
                        dwarfkit::CancelToken());
    }

private:
    // A qr or link element's data is the payload itself (the esr: URI), not
    // a JSON-encoded string with quotes around it; anything structured is
    // serialized. The payload comes from a wallet, so invalid UTF-8 is
    // replaced rather than allowed to throw.
    static std::string DataText(const dwarfkit::json& Data)
    {
        if (Data.is_string())
        {
            return Data.get<std::string>();
        }
        return Data.dump(-1, ' ', false, dwarfkit::json::error_handler_t::replace);
    }

    // Dispatch to the game thread and wait until it ran, so UI ordering
    // matches the kit's flow. The wait is bounded and cancellable: a game
    // thread that stopped ticking must not pin the worker.
    void RunOnGameThread(TFunction<void(UDwarfkitUI*)> Fn, const dwarfkit::CancelToken& Token)
    {
        TWeakObjectPtr<UDwarfkitSubsystem> WeakSubsystem = Subsystem;
        DkUnreal::RunOnGameThreadAndWait(
            [WeakSubsystem, Fn = MoveTemp(Fn)]()
            {
                if (UDwarfkitSubsystem* Sub = WeakSubsystem.Get())
                {
                    if (UDwarfkitUI* Widget = Sub->GetUI().Get())
                    {
                        Fn(Widget);
                    }
                }
            },
            Token, 10000);
    }

    TWeakObjectPtr<UDwarfkitSubsystem> Subsystem;
};
