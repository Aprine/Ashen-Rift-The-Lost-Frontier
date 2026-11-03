#include "AREquipmentComponent.h"
#include "ARPrototype.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "ARRunGameInstance.h"
#include "Kismet/GameplayStatics.h"

UAREquipmentComponent::UAREquipmentComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UAREquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Player = Cast<AARPlayerCharacter>(GetOwner())) Player->Inventory->OnChanged.AddDynamic(this, &UAREquipmentComponent::ValidateOwnership);
}
bool UAREquipmentComponent::IsEquipment(FName ID)
{ return ID == TEXT("IronSword") || ID == TEXT("OakStaff") || ID == TEXT("LeatherArmor") || ID == TEXT("RunicCharm"); }
int32 UAREquipmentComponent::DamageBonus() const { return (Weapon.IsNone() ? 0 : 8) + (Charm.IsNone() ? 0 : 4); }
int32 UAREquipmentComponent::HealthBonus() const { return Armor.IsNone() ? 0 : 20; }
bool UAREquipmentComponent::ToggleEquip(FName ID)
{
    auto* Player = Cast<AARPlayerCharacter>(GetOwner());
    auto* State = Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (!Player || !State || !Player->CanPlay() || State->Phase != EARRunPhase::InDungeon || !IsEquipment(ID) || Player->Inventory->Count(ID) <= 0) return false;
    if (ID == TEXT("IronSword") || ID == TEXT("OakStaff"))
    {
        const FName Allowed = Player->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("OakStaff")) : FName(TEXT("IronSword"));
        if (ID != Allowed) return false;
        Weapon = Weapon == ID ? NAME_None : ID;
    }
    else if (ID == TEXT("LeatherArmor")) Armor = Armor == ID ? NAME_None : ID;
    else Charm = Charm == ID ? NAME_None : ID;
    RefreshHealth();
    Player->ShowNotice(TEXT("Equipment updated"));
    return true;
}
void UAREquipmentComponent::RefreshHealth()
{
    if (auto* Player = Cast<AARPlayerCharacter>(GetOwner()))
    {
        const float Maximum = Player->BaseMaxHealth() + HealthBonus();
        Player->Health->MaxHealth = Maximum;
        // Re-equipping armor raises the maximum but cannot repeatedly heal.
        Player->Health->CurrentHealth = FMath::Min(Player->Health->CurrentHealth, Maximum);
        Player->Health->OnHealthChanged.Broadcast(Player->Health->CurrentHealth, Maximum);
    }
}
void UAREquipmentComponent::ValidateOwnership()
{
    if (auto* Player = Cast<AARPlayerCharacter>(GetOwner()))
    {
        if (!Weapon.IsNone() && Player->Inventory->Count(Weapon) <= 0) Weapon = NAME_None;
        if (!Armor.IsNone() && Player->Inventory->Count(Armor) <= 0) Armor = NAME_None;
        if (!Charm.IsNone() && Player->Inventory->Count(Charm) <= 0) Charm = NAME_None;
        RefreshHealth();
    }
}
void UAREquipmentComponent::ClearEquipment() { Weapon = Armor = Charm = NAME_None; RefreshHealth(); }

int32 AARPlayerCharacter::TotalDamageBonus() const
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    return RunPowerBonus + Equipment->DamageBonus() + (State ? State->PermanentDamageBonus() : 0);
}
void AARPlayerCharacter::EquipWeapon()
{
    if (!Equipment->ToggleEquip(SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword")))
        ShowNotice(TEXT("Collect your class weapon, then press Z"));
}
void AARPlayerCharacter::EquipArmor()
{ if (!Equipment->ToggleEquip(TEXT("LeatherArmor"))) ShowNotice(TEXT("Collect LeatherArmor, then press X")); }
void AARPlayerCharacter::EquipCharm()
{ if (!Equipment->ToggleEquip(TEXT("RunicCharm"))) ShowNotice(TEXT("Collect RunicCharm, then press C")); }
