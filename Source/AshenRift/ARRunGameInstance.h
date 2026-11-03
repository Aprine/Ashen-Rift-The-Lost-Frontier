#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ARCoreTypes.h"
#include "ARRunGameInstance.generated.h"

class UARProfile;
class UARInventoryComponent;
class AARPlayerCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARRunChanged);

UCLASS()
class ASHENRIFT_API UARRunGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    UPROPERTY(BlueprintReadOnly, Category="Run") EARRunPhase Phase = EARRunPhase::Hub;
    UPROPERTY(BlueprintReadOnly, Category="Run") int32 RunGold = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") int32 GoldEarned = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") int32 GoldSpent = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") bool bBossDefeated = false;
    UPROPERTY(BlueprintReadOnly, Category="Run") int32 RunRunes = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") int32 Seed = 0;
    UPROPERTY(BlueprintReadOnly, Category="Run") FARRunSummary Summary;
    UPROPERTY(BlueprintReadOnly, Category="Save") UARProfile* Profile = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="Save") FString LastError;
    UPROPERTY(BlueprintAssignable) FARRunChanged OnChanged;
    UFUNCTION(BlueprintCallable, Category="Run") bool BeginRun(int32 InSeed, UARInventoryComponent* LoadoutInventory = nullptr);
    UPROPERTY(BlueprintReadOnly, Category="Camp") TMap<FName, int32> PreparedLoadout;
    UFUNCTION(BlueprintPure, Category="Camp") int32 TalentRank(FName Talent) const;
    UFUNCTION(BlueprintPure, Category="Camp") int32 TalentCost(FName Talent) const;
    int32 PermanentHealthBonus() const { return TalentRank(TEXT("Vitality")) * 10; }
    int32 PermanentDamageBonus() const { return TalentRank(TEXT("Might")) * 2; }
    UFUNCTION(BlueprintCallable, Category="Camp") bool PurchaseTalent(FName Talent, AARPlayerCharacter* Player);
    UFUNCTION(BlueprintCallable, Category="Camp") bool AdjustLoadout(FName Item, int32 Delta, AARPlayerCharacter* Player);
    void MatchLoadoutToClass(AARPlayerCharacter* Player);
    UFUNCTION(BlueprintCallable, Category="Run") bool AddCurrency(int32 Gold, int32 Runes);
    bool BuyPotion(UARInventoryComponent* Inventory);
    bool BuyPower(AARPlayerCharacter* Player);
    UFUNCTION(BlueprintCallable, Category="Run") bool BeginExtraction();
    UFUNCTION(BlueprintCallable, Category="Run") void CancelExtraction();
    UFUNCTION(BlueprintCallable, Category="Run") bool SettleRun(bool bSuccess, UARInventoryComponent* Inventory);
    UFUNCTION(BlueprintCallable, Category="Save") bool RetrySave();
    UFUNCTION(BlueprintCallable, Category="Run") bool ReturnToHub();
    FString GetProfileSlot() const { return Slot; }
private:
    UPROPERTY() UARProfile* PendingProfile = nullptr;
    UPROPERTY() UARInventoryComponent* PendingInventory = nullptr;
    bool bProfileLoaded = false;
    bool bProfileTransaction = false;
    bool CanManageCamp(AARPlayerCharacter* Player) const;
    bool CommitPendingSave();
    void FinishSettlement();
    FString Slot = TEXT("AshenRift_Profile_v1");
};
