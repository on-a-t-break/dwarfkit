#include "DwarfkitAsyncActions.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{

UDwarfkitSubsystem* FindSubsystem(UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    const UWorld* World = WorldContextObject->GetWorld();
    if (!World || !World->GetGameInstance())
    {
        return nullptr;
    }
    return World->GetGameInstance()->GetSubsystem<UDwarfkitSubsystem>();
}

FDkSessionInfo InfoFor(const std::shared_ptr<dwarfkit::Session>& Session)
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

// An Anchor login waits as long as the user takes to scan a code, so the
// action must survive garbage collection meanwhile; registering with the game
// instance is what SetReadyToDestroy later undoes.
template <class TAction>
TAction* NewAction(UObject* WorldContextObject)
{
    TAction* Action = NewObject<TAction>();
    if (WorldContextObject)
    {
        Action->RegisterWithGameInstance(WorldContextObject);
    }
    Action->Subsystem = FindSubsystem(WorldContextObject);
    return Action;
}

}  // namespace

// ---- Login -----------------------------------------------------------------

UDkLoginAction* UDkLoginAction::Login(UObject* WorldContextObject, const FString& InActor,
                                      const FString& InPermission)
{
    UDkLoginAction* Action = NewAction<UDkLoginAction>(WorldContextObject);
    Action->Actor = InActor;
    Action->Permission = InPermission;
    return Action;
}

void UDkLoginAction::Activate()
{
    if (!Subsystem || !Subsystem->GetKit())
    {
        Completed.Broadcast(FDkSessionInfo(), TEXT("Dwarfkit is not configured"));
        SetReadyToDestroy();
        return;
    }
    // the worker shares ownership of the kit and addresses UObjects weakly:
    // a PIE stop while the wallet is waiting frees nothing underneath it
    std::shared_ptr<dwarfkit::SessionKit> Kit = Subsystem->GetKit();
    TWeakObjectPtr<UDkLoginAction> WeakThis(this);
    TWeakObjectPtr<UDwarfkitSubsystem> WeakSubsystem(Subsystem);
    const std::string ActorStr = TCHAR_TO_UTF8(*Actor);
    const std::string PermissionStr = TCHAR_TO_UTF8(*Permission);

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
              [WeakThis, WeakSubsystem, Kit, ActorStr, PermissionStr]()
              {
                  dwarfkit::LoginOptions Options;
                  if (!ActorStr.empty())
                  {
                      Options.permissionLevel = dwarfkit::PermissionLevel{
                          dwarfkit::Name::from(ActorStr),
                          dwarfkit::Name::from(
                              PermissionStr.empty() ? "active" : PermissionStr)};
                  }
                  auto LoginResult = Kit->login(Options);

                  AsyncTask(ENamedThreads::GameThread,
                            [WeakThis, WeakSubsystem,
                             LoginResult = MoveTemp(LoginResult)]() mutable
                            {
                                UDkLoginAction* Self = WeakThis.Get();
                                if (!Self)
                                {
                                    return;
                                }
                                if (LoginResult)
                                {
                                    if (UDwarfkitSubsystem* Sub = WeakSubsystem.Get())
                                    {
                                        Sub->SetSession(LoginResult->session);
                                    }
                                    Self->Completed.Broadcast(InfoFor(LoginResult->session),
                                                              FString());
                                }
                                else
                                {
                                    Self->Completed.Broadcast(
                                        FDkSessionInfo(),
                                        UTF8_TO_TCHAR(LoginResult.error().message.c_str()));
                                }
                                Self->SetReadyToDestroy();
                            });
              });
}

// ---- Restore ---------------------------------------------------------------

UDkRestoreAction* UDkRestoreAction::Restore(UObject* WorldContextObject)
{
    return NewAction<UDkRestoreAction>(WorldContextObject);
}

void UDkRestoreAction::Activate()
{
    if (!Subsystem || !Subsystem->GetKit())
    {
        Completed.Broadcast(FDkSessionInfo(), TEXT("Dwarfkit is not configured"));
        SetReadyToDestroy();
        return;
    }
    std::shared_ptr<dwarfkit::SessionKit> Kit = Subsystem->GetKit();
    TWeakObjectPtr<UDkRestoreAction> WeakThis(this);
    TWeakObjectPtr<UDwarfkitSubsystem> WeakSubsystem(Subsystem);
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
              [WeakThis, WeakSubsystem, Kit]()
              {
                  auto RestoreResult = Kit->restore();
                  AsyncTask(ENamedThreads::GameThread,
                            [WeakThis, WeakSubsystem,
                             RestoreResult = MoveTemp(RestoreResult)]() mutable
                            {
                                UDkRestoreAction* Self = WeakThis.Get();
                                if (!Self)
                                {
                                    return;
                                }
                                if (RestoreResult && *RestoreResult)
                                {
                                    if (UDwarfkitSubsystem* Sub = WeakSubsystem.Get())
                                    {
                                        Sub->SetSession(*RestoreResult);
                                    }
                                    Self->Completed.Broadcast(InfoFor(*RestoreResult), FString());
                                }
                                else if (RestoreResult)
                                {
                                    Self->Completed.Broadcast(FDkSessionInfo(),
                                                              TEXT("No stored session"));
                                }
                                else
                                {
                                    Self->Completed.Broadcast(
                                        FDkSessionInfo(),
                                        UTF8_TO_TCHAR(
                                            RestoreResult.error().message.c_str()));
                                }
                                Self->SetReadyToDestroy();
                            });
              });
}

// ---- Transact --------------------------------------------------------------

UDkTransactAction* UDkTransactAction::Transact(UObject* WorldContextObject,
                                               const FString& InActionJson, bool bInBroadcast)
{
    UDkTransactAction* Action = NewAction<UDkTransactAction>(WorldContextObject);
    Action->ActionJson = InActionJson;
    Action->bBroadcast = bInBroadcast;
    return Action;
}

void UDkTransactAction::Activate()
{
    if (!Subsystem || !Subsystem->GetSession())
    {
        Completed.Broadcast(FString(), TEXT("No active session"));
        SetReadyToDestroy();
        return;
    }
    std::shared_ptr<dwarfkit::Session> Session = Subsystem->GetSession();
    TWeakObjectPtr<UDkTransactAction> WeakThis(this);
    const std::string ActionText = TCHAR_TO_UTF8(*ActionJson);
    const bool bLocalBroadcast = bBroadcast;

    AsyncTask(
        ENamedThreads::AnyBackgroundThreadNormalTask,
        [WeakThis, Session, ActionText, bLocalBroadcast]()
        {
            const dwarfkit::json Parsed =
                dwarfkit::json::parse(ActionText, nullptr, false);
            FString TransactionId;
            FString Error;
            if (Parsed.is_discarded())
            {
                Error = TEXT("Invalid action JSON");
            }
            else
            {
                dwarfkit::TransactOptions Options;
                Options.broadcast = bLocalBroadcast;
                const auto TransactResult =
                    Session->transact({.action = Parsed}, Options);
                if (TransactResult)
                {
                    if (TransactResult->response &&
                        TransactResult->response->contains("transaction_id") &&
                        (*TransactResult->response)["transaction_id"].is_string())
                    {
                        TransactionId = UTF8_TO_TCHAR(
                            (*TransactResult->response)["transaction_id"]
                                .get<std::string>()
                                .c_str());
                    }
                }
                else
                {
                    Error = UTF8_TO_TCHAR(TransactResult.error().message.c_str());
                }
            }
            AsyncTask(ENamedThreads::GameThread,
                      [WeakThis, TransactionId, Error]()
                      {
                          if (UDkTransactAction* Self = WeakThis.Get())
                          {
                              Self->Completed.Broadcast(TransactionId, Error);
                              Self->SetReadyToDestroy();
                          }
                      });
        });
}
