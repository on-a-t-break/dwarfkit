#include "DwarfkitSampleActor.h"

#include "DwarfkitAsyncActions.h"
#include "Engine/GameInstance.h"

void ADwarfkitSampleActor::BeginPlay()
{
    Super::BeginPlay();

    UGameInstance* GameInstance = GetGameInstance();
    UDwarfkitSubsystem* Subsystem =
        GameInstance ? GameInstance->GetSubsystem<UDwarfkitSubsystem>() : nullptr;
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Dwarfkit: no game instance subsystem in this world"));
        return;
    }
    Subsystem->Configure(AppName, ChainId, ApiUrl);
    // prompts (the login QR) go to the assigned UI; assign one before playing,
    // otherwise the wallet wait has nothing to show
    if (!Subsystem->GetUI().IsValid())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("Dwarfkit: no UDwarfkitUI assigned, the login prompt will not be shown"));
    }

    // the action registers with the game instance, so it survives garbage
    // collection while the wallet is waiting
    UDkLoginAction* LoginAction = UDkLoginAction::Login(this, LoginActor, LoginPermission);
    LoginAction->Completed.AddDynamic(this, &ADwarfkitSampleActor::OnLoggedIn);
    LoginAction->Activate();
}

void ADwarfkitSampleActor::OnLoggedIn(const FDkSessionInfo& Session, const FString& Error)
{
    if (!Error.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Dwarfkit login failed: %s"), *Error);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("Dwarfkit logged in as %s@%s"), *Session.Actor,
           *Session.Permission);

    const FString ActionJson = FString::Printf(
        TEXT("{\"account\":\"eosio.token\",\"name\":\"transfer\","
             "\"authorization\":[{\"actor\":\"%s\",\"permission\":\"%s\"}],"
             "\"data\":{\"from\":\"%s\",\"to\":\"%s\",\"quantity\":\"%s\","
             "\"memo\":\"sent with dwarfkit\"}}"),
        *Session.Actor, *Session.Permission, *Session.Actor, *TransferTo, *Quantity);

    UDkTransactAction* TransactAction = UDkTransactAction::Transact(this, ActionJson, true);
    TransactAction->Completed.AddDynamic(this, &ADwarfkitSampleActor::OnTransacted);
    TransactAction->Activate();
}

void ADwarfkitSampleActor::OnTransacted(const FString& TransactionId, const FString& Error)
{
    if (!Error.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Dwarfkit transact failed: %s"), *Error);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("Dwarfkit transaction broadcast: %s"), *TransactionId);
}
