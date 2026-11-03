#include "ARRunGameInstance.h"
#include "ARProfile.h"
#include "ARInventoryComponent.h"
#include "AREquipmentComponent.h"
#include "ARPrototype.h"
#include "ARHealthComponent.h"
#include "ARCombatFeedback.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UARRunGameInstance::Init()
{
    Super::Init();
#if !UE_BUILD_SHIPPING
    // Integration tests never read or overwrite the player's real profile.
    if (FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")))
        Slot = TEXT("AshenRift_Automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
#endif
    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        Profile = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        if (!Profile || Profile->SaveVersion != 1 || Profile->BankGold < 0 || Profile->BankRunes < 0)
        {
            Profile = nullptr;
            LastError = TEXT("Profile load failed or unsupported version. Existing save was not overwritten.");
            return;
        }
        for (const auto& Pair : Profile->Stash)
            if (Pair.Key.IsNone() || Pair.Value <= 0) { Profile = nullptr; break; }
        if (Profile)
            for (const auto& Pair : Profile->TalentRanks)
                if ((Pair.Key != TEXT("Vitality") && Pair.Key != TEXT("Might")) || Pair.Value < 0 || Pair.Value > 3)
                { Profile = nullptr; break; }
        if (!Profile)
        { LastError = TEXT("Invalid stash or talents. Existing save was not overwritten."); return; }
    }
    else Profile = Cast<UARProfile>(UGameplayStatics::CreateSaveGameObject(UARProfile::StaticClass()));
    bProfileLoaded = Profile != nullptr;
}
bool UARRunGameInstance::BeginRun(int32 InSeed, UARInventoryComponent* LoadoutInventory)
{
    if (!bProfileLoaded || bProfileTransaction || Phase != EARRunPhase::Hub) return false;
    TGuardValue<bool> Guard(bProfileTransaction, true);
    if (LoadoutInventory && (!IsValid(LoadoutInventory) || LoadoutInventory->Items.Num() != 0 ||
        !IsValid(LoadoutInventory->GetOwner()) || LoadoutInventory->GetOwner()->GetGameInstance() != this)) return false;
    if (PreparedLoadout.Num() > 0)
    {
        auto* Player = LoadoutInventory ? Cast<AARPlayerCharacter>(LoadoutInventory->GetOwner()) : nullptr;
        if (!CanManageCamp(Player) || Player->Inventory != LoadoutInventory || PreparedLoadout.Num() > LoadoutInventory->Capacity)
        { LastError = TEXT("Cannot deploy: return to camp with an empty backpack and enough slots."); return false; }
        const FName OtherWeapon = Player->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("IronSword")) : FName(TEXT("OakStaff"));
        auto* Candidate = DuplicateObject<UARProfile>(Profile, this);
        for (const auto& Pair : PreparedLoadout)
        {
            const int32 Limit = Pair.Key == TEXT("Potion") ? 5 : 1;
            if ((Pair.Key != TEXT("Potion") && !UAREquipmentComponent::IsEquipment(Pair.Key)) || Pair.Key == OtherWeapon ||
                Pair.Value <= 0 || Pair.Value > Limit || Pair.Value > Profile->Stash.FindRef(Pair.Key) || !LoadoutInventory->CanAdd(Pair.Key, Pair.Value))
            { LastError = TEXT("Cannot deploy: prepared items are unavailable or exceed backpack limits."); return false; }
            const int32 Left = Candidate->Stash.FindRef(Pair.Key) - Pair.Value;
            if (Left == 0) Candidate->Stash.Remove(Pair.Key); else Candidate->Stash.Add(Pair.Key, Left);
        }
        // Persist the debit before entering danger. Failure leaves the entire plan and stash intact.
        if (!UGameplayStatics::SaveGameToSlot(Candidate, Slot, 0))
        { LastError = TEXT("Could not save loadout. Nothing was withdrawn; press E to retry."); return false; }
        Phase = EARRunPhase::Settling;
        Profile = Candidate;
        LoadoutInventory->Items = PreparedLoadout;
        PreparedLoadout.Empty();
        LoadoutInventory->OnChanged.Broadcast();
    }
    RunGold = RunRunes = 0;
    GoldEarned = GoldSpent = 0;
    bBossDefeated = false;
    Seed = InSeed;
    Summary = FARRunSummary();
    LastError.Empty();
    Phase = EARRunPhase::InDungeon;
    OnChanged.Broadcast();
    return true;
}
bool UARRunGameInstance::AddCurrency(int32 Gold, int32 Runes)
{
    if (Phase != EARRunPhase::InDungeon || Gold < 0 || Runes < 0 ||
        Gold > MAX_int32 - RunGold || Gold > MAX_int32 - GoldEarned || Runes > MAX_int32 - RunRunes) return false;
    RunGold += Gold;
    GoldEarned += Gold;
    RunRunes += Runes;
    OnChanged.Broadcast();
    return true;
}
bool UARRunGameInstance::BuyPotion(UARInventoryComponent* Inventory)
{
    const int32 Price = 8;
    if (Phase != EARRunPhase::InDungeon || !IsValid(Inventory) || RunGold < Price || !Inventory->CanAdd(TEXT("Potion"), 1)) return false;
    // Debit before dispatching inventory events so observers see a committed price.
    RunGold -= Price;
    GoldSpent += Price;
    if (!Inventory->TryAdd(TEXT("Potion"), 1))
    { RunGold += Price; GoldSpent -= Price; return false; }
    OnChanged.Broadcast();
    return true;
}
bool UARRunGameInstance::BuyPower(AARPlayerCharacter* Player)
{
    const int32 Price = 15;
    if (Phase != EARRunPhase::InDungeon || !IsValid(Player) || Player->Health->bDead || RunGold < Price || Player->RunPowerBonus >= 15) return false;
    RunGold -= Price;
    GoldSpent += Price;
    Player->RunPowerBonus += 5;
    OnChanged.Broadcast();
    return true;
}
bool UARRunGameInstance::BeginExtraction()
{
    if (Phase != EARRunPhase::InDungeon) return false;
    Phase = EARRunPhase::Extracting;
    OnChanged.Broadcast();
    return true;
}
void UARRunGameInstance::CancelExtraction()
{
    if (Phase != EARRunPhase::Extracting) return;
    Phase = EARRunPhase::InDungeon;
    OnChanged.Broadcast();
}
bool UARRunGameInstance::SettleRun(bool bSuccess, UARInventoryComponent* Inventory)
{
    // Death is valid while exploring or extracting; success requires an extraction.
    if (!bProfileLoaded || bProfileTransaction || !IsValid(Inventory) ||
        (bSuccess && Phase != EARRunPhase::Extracting) ||
        (Phase != EARRunPhase::InDungeon && Phase != EARRunPhase::Extracting)) return false;
    Phase = EARRunPhase::Settling;
    Summary.bSuccess = bSuccess;
    Summary.EarnedGold = GoldEarned;
    Summary.SpentGold = GoldSpent;
    Summary.bBossDefeated = bBossDefeated;
    Summary.EarnedRunes = RunRunes;
    Summary.BankedGold = bSuccess ? RunGold : 0;
    Summary.LostGold = bSuccess ? 0 : RunGold;
    Summary.Loot = Inventory->Items;
    PendingInventory = Inventory;
    if (!bSuccess) { FinishSettlement(); return true; }
    PendingProfile = DuplicateObject<UARProfile>(Profile, this);
    if (RunGold > MAX_int32 - Profile->BankGold || RunRunes > MAX_int32 - Profile->BankRunes)
    {
        LastError = TEXT("Bank currency overflow. Settlement remains locked.");
        PendingProfile = nullptr;
        OnChanged.Broadcast();
        return false;
    }
    PendingProfile->BankGold += RunGold;
    PendingProfile->BankRunes += RunRunes;
    for (const auto& Pair : Summary.Loot)
    {
        const int32 Existing = PendingProfile->Stash.FindRef(Pair.Key);
        if (Pair.Key.IsNone() || Pair.Value <= 0 || Existing < 0 || Pair.Value > MAX_int32 - Existing)
        {
            LastError = TEXT("Invalid stash quantity. Settlement remains locked.");
            PendingProfile = nullptr;
            OnChanged.Broadcast();
            return false;
        }
        PendingProfile->Stash.Add(Pair.Key, Existing + Pair.Value);
    }
    return CommitPendingSave();
}
bool UARRunGameInstance::CommitPendingSave()
{
    if (!PendingProfile || Phase != EARRunPhase::Settling) return false;
    if (!UGameplayStatics::SaveGameToSlot(PendingProfile, Slot, 0))
    {
        LastError = TEXT("Could not save extracted loot. RetrySave without starting a new run.");
        OnChanged.Broadcast();
        return false;
    }
    Profile = PendingProfile;
    PendingProfile = nullptr;
    FinishSettlement();
    return true;
}
bool UARRunGameInstance::RetrySave() { return CommitPendingSave(); }
void UARRunGameInstance::FinishSettlement()
{
    RunGold = RunRunes = 0;
    Phase = EARRunPhase::Result;
    LastError.Empty();
    UARInventoryComponent* Inventory = PendingInventory;
    PendingInventory = nullptr;
    if (IsValid(Inventory)) Inventory->Clear();
    if (Summary.bSuccess)
        if (auto* Audio = UARFeedbackSubsystem::Get(this))
            if (auto* Player = UGameplayStatics::GetPlayerPawn(this, 0)) Audio->PlayCue(EARFeedbackCue::Extract, Player->GetActorLocation());
    OnChanged.Broadcast();
}
bool UARRunGameInstance::ReturnToHub()
{
    if (Phase != EARRunPhase::Result) return false;
    Phase = EARRunPhase::Hub;
    OnChanged.Broadcast();
    return true;
}
