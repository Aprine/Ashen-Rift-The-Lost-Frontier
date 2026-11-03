#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARInventoryChanged);

// M1 inventory: one slot per distinct item ID. Equipment instances come later.
UCLASS(ClassGroup=(AshenRift), meta=(BlueprintSpawnableComponent))
class ASHENRIFT_API UARInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARInventoryComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 Capacity = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 MaxStack = 99;
    UPROPERTY(BlueprintReadOnly, Category="Inventory") TMap<FName, int32> Items;
    UPROPERTY(BlueprintAssignable) FARInventoryChanged OnChanged;
    UFUNCTION(BlueprintPure, Category="Inventory") int32 Count(FName ItemID) const;
    UFUNCTION(BlueprintPure, Category="Inventory") bool CanAdd(FName ItemID, int32 Quantity) const;
    UFUNCTION(BlueprintCallable, Category="Inventory") bool TryAdd(FName ItemID, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Inventory") bool TryRemove(FName ItemID, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Inventory") void Clear();
};
