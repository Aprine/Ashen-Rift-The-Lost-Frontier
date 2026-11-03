#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AREquipmentComponent.generated.h"

UCLASS(ClassGroup=(AshenRift), meta=(BlueprintSpawnableComponent))
class ASHENRIFT_API UAREquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAREquipmentComponent();
    virtual void BeginPlay() override;
    UPROPERTY(BlueprintReadOnly) FName Weapon;
    UPROPERTY(BlueprintReadOnly) FName Armor;
    UPROPERTY(BlueprintReadOnly) FName Charm;
    UFUNCTION(BlueprintPure) static bool IsEquipment(FName ID);
    UFUNCTION(BlueprintCallable) bool ToggleEquip(FName ID);
    UFUNCTION(BlueprintCallable) void ClearEquipment();
    UFUNCTION(BlueprintPure) int32 DamageBonus() const;
    UFUNCTION(BlueprintPure) int32 HealthBonus() const;
    void RefreshHealth();
private:
    UFUNCTION() void ValidateOwnership();
};
