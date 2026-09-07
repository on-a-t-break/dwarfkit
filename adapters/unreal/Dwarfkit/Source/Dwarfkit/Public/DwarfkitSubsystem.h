// GameInstance subsystem owning the Dwarfkit SessionKit (BLUEPRINT.md 8.1).
// All kit calls run on a background thread; results come back through the
// async action delegates on the game thread. Workers share ownership of the
// kit and observe its CancelToken, so Deinitialize and Cancel unwind them
// rather than freeing the kit underneath them.
#pragma once

#include <memory>

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

THIRD_PARTY_INCLUDES_START
#include <dwarfkit/core/cancel.hpp>
#include <dwarfkit/plugins/wallet/anchor.hpp>
#include <dwarfkit/session.hpp>
THIRD_PARTY_INCLUDES_END

#include "DwarfkitUI.h"

#include "DwarfkitSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FDkSessionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Dwarfkit")
    FString ChainId;

    UPROPERTY(BlueprintReadOnly, Category = "Dwarfkit")
    FString Actor;

    UPROPERTY(BlueprintReadOnly, Category = "Dwarfkit")
    FString Permission;
};

UCLASS()
class DWARFKIT_API UDwarfkitSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Configure the kit for a chain before logging in. ChainId is the 64-char
    // hex id; Url the API node. Calling it again retires the previous
    // configuration's workers.
    UFUNCTION(BlueprintCallable, Category = "Dwarfkit")
    void Configure(const FString& AppName, const FString& ChainId, const FString& Url);

    // Assign the Blueprint UI receiving prompts; may be set before or after
    // Configure.
    UFUNCTION(BlueprintCallable, Category = "Dwarfkit")
    void SetUI(UDwarfkitUI* InUI);

    // Interrupts whatever login, restore or transact is waiting on the
    // wallet; the kit stays configured and usable.
    UFUNCTION(BlueprintCallable, Category = "Dwarfkit")
    void Cancel();

    UFUNCTION(BlueprintPure, Category = "Dwarfkit")
    bool HasSession() const;

    UFUNCTION(BlueprintPure, Category = "Dwarfkit")
    FDkSessionInfo GetSessionInfo() const;

    // Native access for the async actions and the UI bridge.
    std::shared_ptr<dwarfkit::SessionKit> GetKit() const { return Kit; }
    dwarfkit::CancelToken GetToken() const { return Token; }
    std::shared_ptr<dwarfkit::Session> GetSession() const { return Session; }
    void SetSession(std::shared_ptr<dwarfkit::Session> InSession) { Session = InSession; }
    // Read from workers, so guarded; resolve the pointer on the game thread.
    TWeakObjectPtr<UDwarfkitUI> GetUI() const;

private:
    void Build();

    std::shared_ptr<dwarfkit::SessionKit> Kit;
    dwarfkit::CancelToken Token;
    std::shared_ptr<dwarfkit::Session> Session;
    TWeakObjectPtr<UDwarfkitUI> UI;
    mutable FCriticalSection UILock;
    FString ConfiguredAppName;
    FString ConfiguredChainId;
    FString ConfiguredUrl;
};
