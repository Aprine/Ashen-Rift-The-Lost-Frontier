#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARHealthComponent.generated.h"

class UDamageType;
class AController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARHealthChanged, float, Current, float, Maximum);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARHealthEvent);

UCLASS(ClassGroup=(AshenRift), meta=(BlueprintSpawnableComponent))
class ASHENRIFT_API UARHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARHealthComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="1")) float MaxHealth = 100.f;
    UPROPERTY(BlueprintReadOnly, Category="Health") float CurrentHealth = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Health") bool bDead = false;
    UPROPERTY(BlueprintAssignable) FARHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable) FARHealthEvent OnDied;
    UPROPERTY(BlueprintAssignable) FARHealthEvent OnDamaged;
    UFUNCTION(BlueprintCallable, Category="Health") void ReceiveDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="Health") float Heal(float Amount);
    UFUNCTION(BlueprintCallable, Category="Health") void ResetHealth();
protected:
    virtual void BeginPlay() override;
    UFUNCTION() void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* Type,
                                    AController* Instigator, AActor* Causer);
};
