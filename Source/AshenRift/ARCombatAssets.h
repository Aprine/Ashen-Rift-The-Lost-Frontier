#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARCombatAssets.generated.h"

UCLASS()
class ASHENRIFT_API UARCombatAssets : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor") static bool CreateCombatAnimations();
};
