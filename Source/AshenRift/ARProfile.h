#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARProfile.generated.h"

UCLASS()
class ASHENRIFT_API UARProfile : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SaveVersion = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 BankGold = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 BankRunes = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<FName, int32> Stash;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<FName, int32> TalentRanks;
};
