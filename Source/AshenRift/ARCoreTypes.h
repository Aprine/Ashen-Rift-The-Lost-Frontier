#pragma once
#include "CoreMinimal.h"
#include "ARCoreTypes.generated.h"

UENUM(BlueprintType)
enum class EARRunPhase : uint8 { Hub, InDungeon, Extracting, Settling, Result };

UENUM(BlueprintType)
enum class EARPlayerClass : uint8 { Warrior, Mage };

USTRUCT(BlueprintType)
struct FARRunSummary
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly) int32 EarnedGold = 0;
    UPROPERTY(BlueprintReadOnly) int32 SpentGold = 0;
    UPROPERTY(BlueprintReadOnly) int32 BankedGold = 0;
    UPROPERTY(BlueprintReadOnly) int32 LostGold = 0;
    UPROPERTY(BlueprintReadOnly) int32 EarnedRunes = 0;
    UPROPERTY(BlueprintReadOnly) bool bBossDefeated = false;
    UPROPERTY(BlueprintReadOnly) TMap<FName, int32> Loot;
};
