#include "DwarfkitSubsystem.h"

#include "DkUnrealProviders.h"
#include "DkUnrealUserInterface.h"

void UDwarfkitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UDwarfkitSubsystem::Deinitialize()
{
    // workers keep their own shared_ptr to the kit and unwind on the token,
    // so nothing here frees memory a worker is still inside
    Token.cancel();
    Session.reset();
    Kit.reset();
    Super::Deinitialize();
}

void UDwarfkitSubsystem::Build()
{
    const auto ParsedId =
        dwarfkit::Checksum256::from(std::string(TCHAR_TO_UTF8(*ConfiguredChainId)));
    if (!ParsedId)
    {
        UE_LOG(LogTemp, Error, TEXT("Dwarfkit: invalid chain id %s"), *ConfiguredChainId);
        return;
    }
    // a token cannot be reset and the wallet plugin holds it by value, so a
    // rebuild always starts from a fresh one
    Token = dwarfkit::CancelToken();

    dwarfkit::SessionKitArgs Args;
    Args.appName = TCHAR_TO_UTF8(*ConfiguredAppName);
    Args.chains = {dwarfkit::ChainDefinition::from(
        {.id = *ParsedId, .url = std::string(TCHAR_TO_UTF8(*ConfiguredUrl))})};
    Args.ui = std::make_shared<FDkUnrealUserInterface>(TWeakObjectPtr<UDwarfkitSubsystem>(this));

    dwarfkit::WalletPluginAnchorOptions AnchorOptions;
    AnchorOptions.buoyWs = std::make_shared<FDkUnrealWebSocketProvider>(Token);
    AnchorOptions.token = Token;
    Args.walletPlugins = {std::make_shared<dwarfkit::WalletPluginAnchor>(AnchorOptions)};

    dwarfkit::SessionKitOptions Options;
    Options.fetch = std::make_shared<FDkUnrealFetchProvider>(Token);
    Options.storage = std::make_shared<FDkUnrealStorage>();

    Kit = std::make_shared<dwarfkit::SessionKit>(Args, Options);
}

void UDwarfkitSubsystem::Configure(const FString& AppName, const FString& ChainId,
                                   const FString& Url)
{
    ConfiguredAppName = AppName;
    ConfiguredChainId = ChainId;
    ConfiguredUrl = Url;
    Token.cancel();
    Build();
}

void UDwarfkitSubsystem::Cancel()
{
    if (!Kit)
    {
        return;
    }
    Token.cancel();
    Build();
}

void UDwarfkitSubsystem::SetUI(UDwarfkitUI* InUI)
{
    FScopeLock Lock(&UILock);
    UI = InUI;
}

TWeakObjectPtr<UDwarfkitUI> UDwarfkitSubsystem::GetUI() const
{
    FScopeLock Lock(&UILock);
    return UI;
}

bool UDwarfkitSubsystem::HasSession() const
{
    return Session != nullptr;
}

FDkSessionInfo UDwarfkitSubsystem::GetSessionInfo() const
{
    FDkSessionInfo Info;
    if (Session)
    {
        Info.ChainId = UTF8_TO_TCHAR(Session->chain.id.hexString().c_str());
        Info.Actor = UTF8_TO_TCHAR(Session->actor().toString().c_str());
        Info.Permission = UTF8_TO_TCHAR(Session->permission().toString().c_str());
    }
    return Info;
}
