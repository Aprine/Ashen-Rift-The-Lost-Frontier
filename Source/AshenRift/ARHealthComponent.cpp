#include "ARHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UARHealthComponent::UARHealthComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UARHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    ResetHealth();
    if (GetOwner()) GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UARHealthComponent::HandleAnyDamage);
}
void UARHealthComponent::HandleAnyDamage(AActor*, float Damage, const UDamageType*, AController*, AActor*)
{ ReceiveDamage(Damage); }
void UARHealthComponent::ReceiveDamage(float Amount)
{
    if (bDead || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    if (DamageImmuneUntil > 0.f && GetWorld() && GetWorld()->GetTimeSeconds() < DamageImmuneUntil) return;
    CurrentHealth = FMath::Max(0.f, CurrentHealth - Amount);
    // Lock death before dispatching events; listeners may cause more damage.
    const bool bKilled = CurrentHealth <= 0.f;
    bDead = bKilled;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    OnDamaged.Broadcast();
    if (bKilled) OnDied.Broadcast();
}
void UARHealthComponent::SetDamageImmunity(float Duration)
{
    const auto* World = GetWorld();
    DamageImmuneUntil = World && Duration > 0.f ? World->GetTimeSeconds() + Duration : 0.f;
}
float UARHealthComponent::Heal(float Amount)
{
    if (bDead || !FMath::IsFinite(Amount) || Amount <= 0.f) return 0.f;
    const float Before = CurrentHealth;
    CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
    if (CurrentHealth != Before) OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    return CurrentHealth - Before;
}
void UARHealthComponent::ResetHealth()
{
    MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.f, MaxHealth) : 100.f;
    bDead = false;
    DamageImmuneUntil = 0.f;
    CurrentHealth = MaxHealth;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
