#include "ARRunGameInstance.h"
#include "ARPrototype.h"
#include "ARProfile.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "AREquipmentComponent.h"
#include "Kismet/GameplayStatics.h"

bool UARRunGameInstance::CanManageCamp(AARPlayerCharacter* Player) const
{
    return bProfileLoaded && Profile && IsValid(Player) && Player->GetGameInstance() == this && Player->IsAtCamp();
}
int32 UARRunGameInstance::TalentRank(FName Talent) const
{
    if (!Profile || (Talent != TEXT("Vitality") && Talent != TEXT("Might"))) return 0;
    return FMath::Clamp(Profile->TalentRanks.FindRef(Talent), 0, 3);
}
int32 UARRunGameInstance::TalentCost(FName Talent) const
{
    if ((Talent != TEXT("Vitality") && Talent != TEXT("Might")) || TalentRank(Talent) >= 3) return 0;
    return TalentRank(Talent) + 1;
}
bool UARRunGameInstance::PurchaseTalent(FName Talent, AARPlayerCharacter* Player)
{
    const int32 Cost = TalentCost(Talent);
    if (bProfileTransaction || !CanManageCamp(Player) || Cost <= 0 || Profile->BankRunes < Cost) return false;
    TGuardValue<bool> Guard(bProfileTransaction, true);
    auto* Candidate = DuplicateObject<UARProfile>(Profile, this);
    Candidate->BankRunes -= Cost;
    Candidate->TalentRanks.Add(Talent, TalentRank(Talent) + 1);
    if (!UGameplayStatics::SaveGameToSlot(Candidate, Slot, 0))
    { LastError = TEXT("Could not save talent. No runes were spent; retry the purchase."); return false; }
    Profile = Candidate;
    LastError.Empty();
    Player->ResetCombatForRun();
    OnChanged.Broadcast();
    return true;
}
bool UARRunGameInstance::AdjustLoadout(FName Item, int32 Delta, AARPlayerCharacter* Player)
{
    if (bProfileTransaction || !CanManageCamp(Player) || (Delta != 1 && Delta != -1) ||
        (Item != TEXT("Potion") && !UAREquipmentComponent::IsEquipment(Item))) return false;
    const int32 Count = PreparedLoadout.FindRef(Item);
    const FName OtherWeapon = Player->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("IronSword")) : FName(TEXT("OakStaff"));
    const int32 Limit = Item == TEXT("Potion") ? 5 : 1;
    const int32 NewCount = Count + Delta;
    if (NewCount < 0 || NewCount > Limit || NewCount > Profile->Stash.FindRef(Item) || (Delta > 0 && Item == OtherWeapon)) return false;
    TGuardValue<bool> Guard(bProfileTransaction, true);
    if (NewCount == 0) PreparedLoadout.Remove(Item); else PreparedLoadout.Add(Item, NewCount);
    LastError.Empty();
    OnChanged.Broadcast();
    return true;
}
void UARRunGameInstance::MatchLoadoutToClass(AARPlayerCharacter* Player)
{
    if (bProfileTransaction || !CanManageCamp(Player)) return;
    TGuardValue<bool> Guard(bProfileTransaction, true);
    PreparedLoadout.Remove(Player->SelectedClass == EARPlayerClass::Mage ? TEXT("IronSword") : TEXT("OakStaff"));
    OnChanged.Broadcast();
}

float AARPlayerCharacter::BaseMaxHealth() const
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    return (SelectedClass == EARPlayerClass::Mage ? 75.f : 100.f) + (State ? State->PermanentHealthBonus() : 0);
}
bool AARPlayerCharacter::IsAtCamp() const
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    return State && State->Phase == EARRunPhase::Hub && !Health->bDead &&
        FVector::Dist(GetActorLocation(), FVector(-700, 0, 100)) <= 650;
}
FName AARPlayerCharacter::CampItem(int32 Row)
{
    static const FName Items[] = { TEXT("IronSword"), TEXT("OakStaff"), TEXT("LeatherArmor"), TEXT("RunicCharm"), TEXT("Potion") };
    return Items[FMath::Clamp(Row, 0, 4)];
}
void AARPlayerCharacter::ToggleCamp()
{
    if (!IsAtCamp()) { ShowNotice(TEXT("Return to camp to manage talents and supplies")); return; }
    bCampOpen = !bCampOpen;
    bInventoryVisible = false;
}
void AARPlayerCharacter::CampPrevious() { if (bCampOpen && IsAtCamp()) CampRow = (CampRow + 4) % 5; }
void AARPlayerCharacter::CampNext() { if (bCampOpen && IsAtCamp()) CampRow = (CampRow + 1) % 5; }
void AARPlayerCharacter::CampAdd()
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    if (bCampOpen && State && !State->AdjustLoadout(CampItem(CampRow), 1, this))
        ShowNotice(TEXT("Cannot prepare: check stash, class or carry limit"));
}
void AARPlayerCharacter::CampRemove()
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    if (bCampOpen && State) State->AdjustLoadout(CampItem(CampRow), -1, this);
}
void AARPlayerCharacter::BuyVitality()
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    if (bCampOpen && State) ShowNotice(State->PurchaseTalent(TEXT("Vitality"), this) ? TEXT("Vitality permanently upgraded") : TEXT("Cannot upgrade: check banked runes or rank cap"));
}
void AARPlayerCharacter::BuyMight()
{
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    if (bCampOpen && State) ShowNotice(State->PurchaseTalent(TEXT("Might"), this) ? TEXT("Might permanently upgraded") : TEXT("Cannot upgrade: check banked runes or rank cap"));
}
