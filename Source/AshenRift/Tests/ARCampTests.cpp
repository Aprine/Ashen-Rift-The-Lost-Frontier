#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../ARPrototype.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../AREquipmentComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARCampTest, "AshenRift.Integration.Camp",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARCampCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    int32 CaptureFrames = 0;
    bool bCaptureRequested = false;
public:
    explicit FARCampCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* Player = World ? Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
        auto* State = World ? Cast<UARRunGameInstance>(World->GetGameInstance()) : nullptr;
        if (!Player || !State || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Run L_RandomDungeon with -game -ARTestProfile.")); return true; }
        auto* Mode = Cast<AARPrototypeGameMode>(World->GetAuthGameMode());
        if (!Mode) { Test->AddError(TEXT("Camp test requires prototype mode.")); return true; }
        if (Stage == 0)
        {
            Test->TestFalse(TEXT("Cannot buy talents without banked runes"), State->PurchaseTalent(TEXT("Vitality"), Player));
            Test->TestFalse(TEXT("Unknown talent rejected"), State->PurchaseTalent(TEXT("Unknown"), Player));
            Test->TestFalse(TEXT("Cannot prepare an unowned item"), State->AdjustLoadout(TEXT("IronSword"), 1, Player));
            Player->SetActorLocation(FVector(600, 0, 100));
            Test->TestFalse(TEXT("Camp cannot open outside camp"), Player->IsAtCamp());
            Player->SetActorLocation(FVector(-700, 0, 100));
            Test->TestTrue(TEXT("Initial run starts"), Mode->StartDungeon(Player));
            Mode->ClearRunActors();
            State->AddCurrency(30, 12);
            Player->Inventory->TryAdd(TEXT("IronSword"), 2);
            Player->Inventory->TryAdd(TEXT("OakStaff"), 1);
            Player->Inventory->TryAdd(TEXT("LeatherArmor"), 2);
            Player->Inventory->TryAdd(TEXT("RunicCharm"), 1);
            Player->Inventory->TryAdd(TEXT("Potion"), 5);
            Test->TestFalse(TEXT("Cannot buy talent inside dungeon"), State->PurchaseTalent(TEXT("Vitality"), Player));
            State->BeginExtraction();
            Test->TestTrue(TEXT("Rewards extracted through normal settlement"), State->SettleRun(true, Player->Inventory));
            Test->TestFalse(TEXT("Cannot prepare items on result screen"), State->AdjustLoadout(TEXT("IronSword"), 1, Player));
            Player->ReturnOrRetry();
            for (FName Talent : { FName(TEXT("Vitality")), FName(TEXT("Might")) })
            {
                for (int32 Rank = 1; Rank <= 3; ++Rank)
                {
                    Test->TestEqual(TEXT("Increasing talent price"), State->TalentCost(Talent), Rank);
                    Test->TestTrue(TEXT("Affordable talent saved"), State->PurchaseTalent(Talent, Player));
                    Test->TestEqual(TEXT("Rank increases exactly once"), State->TalentRank(Talent), Rank);
                }
                Test->TestFalse(TEXT("Talent rank cap blocks purchase"), State->PurchaseTalent(Talent, Player));
            }
            Test->TestEqual(TEXT("Both trees cost exactly twelve runes"), State->Profile->BankRunes, 0);
            Test->TestEqual(TEXT("Warrior permanent max health"), Player->Health->MaxHealth, 130.f);
            Test->TestEqual(TEXT("Permanent damage reaches gameplay"), Player->TotalDamageBonus(), 6);
            auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestTrue(TEXT("Talents persisted with compatible version one"), Saved && Saved->SaveVersion == 1 && Saved->TalentRanks.FindRef(TEXT("Vitality")) == 3 && Saved->TalentRanks.FindRef(TEXT("Might")) == 3 && Saved->BankRunes == 0);
            if (Saved) State->Profile = Saved;
            Player->SetActorLocation(FVector(600, 0, 100));
            Test->TestFalse(TEXT("Remote stash access rejected"), State->AdjustLoadout(TEXT("IronSword"), 1, Player));
            Player->SetActorLocation(FVector(-700, 0, 100));
            Test->TestTrue(TEXT("Prepare sword"), State->AdjustLoadout(TEXT("IronSword"), 1, Player));
            Test->TestFalse(TEXT("Only one of each gear can be carried"), State->AdjustLoadout(TEXT("IronSword"), 1, Player));
            Test->TestFalse(TEXT("Wrong class weapon cannot be prepared"), State->AdjustLoadout(TEXT("OakStaff"), 1, Player));
            Player->SelectClass(EARPlayerClass::Mage);
            Test->TestEqual(TEXT("Mage receives shared vitality"), Player->Health->MaxHealth, 105.f);
            Test->TestEqual(TEXT("Changing class removes incompatible plan"), State->PreparedLoadout.FindRef(TEXT("IronSword")), 0);
            State->AdjustLoadout(TEXT("OakStaff"), 1, Player);
            Player->SelectClass(EARPlayerClass::Warrior);
            Test->TestEqual(TEXT("Changing back removes staff plan"), State->PreparedLoadout.FindRef(TEXT("OakStaff")), 0);
            State->AdjustLoadout(TEXT("IronSword"), 1, Player);
            State->AdjustLoadout(TEXT("LeatherArmor"), 1, Player);
            State->AdjustLoadout(TEXT("RunicCharm"), 1, Player);
            State->AdjustLoadout(TEXT("Potion"), 1, Player);
            State->AdjustLoadout(TEXT("Potion"), 1, Player);
            State->AdjustLoadout(TEXT("Potion"), -1, Player);
            Test->TestEqual(TEXT("Removing a planned item only edits selection"), State->PreparedLoadout.FindRef(TEXT("Potion")), 1);
            State->AdjustLoadout(TEXT("Potion"), 1, Player);
            Test->TestEqual(TEXT("Preparation does not debit stash"), State->Profile->Stash.FindRef(TEXT("IronSword")), 2);
            Test->TestFalse(TEXT("Prepared gear requires destination inventory"), State->BeginRun(1));
            Player->Inventory->Capacity = 1;
            Test->TestFalse(TEXT("Insufficient slots rejects entire deployment"), Mode->StartDungeon(Player));
            Test->TestEqual(TEXT("Rejected deployment preserves stash"), State->Profile->Stash.FindRef(TEXT("IronSword")), 2);
            Test->TestEqual(TEXT("Rejected deployment preserves plan"), State->PreparedLoadout.Num(), 4);
            Test->TestTrue(TEXT("Rejected deployment remains in camp"), State->Phase == EARRunPhase::Hub);
            Player->Inventory->Capacity = 12;
            Player->Inventory->TryAdd(TEXT("Potion"), 1);
            Test->TestFalse(TEXT("Nonempty destination rejects deployment"), Mode->StartDungeon(Player));
            Player->Inventory->Clear();
            Test->TestTrue(TEXT("Prepared loadout deploys"), Mode->StartDungeon(Player));
            Mode->ClearRunActors();
            Test->TestEqual(TEXT("Only one sword withdrawn"), State->Profile->Stash.FindRef(TEXT("IronSword")), 1);
            Test->TestEqual(TEXT("Only selected potions withdrawn"), State->Profile->Stash.FindRef(TEXT("Potion")), 3);
            Test->TestEqual(TEXT("Transferred potions exist in run"), Player->Inventory->Count(TEXT("Potion")), 2);
            Test->TestEqual(TEXT("Plan consumed exactly once"), State->PreparedLoadout.Num(), 0);
            Test->TestTrue(TEXT("Prepared gear auto equips"), Player->Equipment->Weapon == TEXT("IronSword") && Player->Equipment->Armor == TEXT("LeatherArmor") && Player->Equipment->Charm == TEXT("RunicCharm"));
            Test->TestEqual(TEXT("Permanent and equipment HP stack"), Player->Health->MaxHealth, 150.f);
            Test->TestEqual(TEXT("Prepared armor starts with full HP"), Player->Health->CurrentHealth, 150.f);
            Test->TestEqual(TEXT("Permanent and equipment damage stack"), Player->TotalDamageBonus(), 18);
            Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestTrue(TEXT("Withdrawal was saved before combat"), Saved && Saved->Stash.FindRef(TEXT("IronSword")) == 1 && Saved->Stash.FindRef(TEXT("Potion")) == 3);
            auto* Dummy = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), Player->GetActorLocation() + FVector(150, 0, 0), FRotator::ZeroRotator);
            if (Dummy) { Player->Attack(); Test->TestEqual(TEXT("Talent and gear affect actual damage"), Dummy->Health->CurrentHealth, 17.f); Dummy->Destroy(); }
            else Test->AddError(TEXT("Could not spawn damage target."));
            State->AddCurrency(9, 5);
            Player->Health->ReceiveDamage(1000);
            Test->TestEqual(TEXT("Death clears carried gear"), Player->Inventory->Items.Num(), 0);
            Test->TestEqual(TEXT("Death never refunds withdrawn gear"), State->Profile->Stash.FindRef(TEXT("IronSword")), 1);
            Test->TestEqual(TEXT("Untouched stash remains safe"), State->Profile->Stash.FindRef(TEXT("LeatherArmor")), 1);
            Test->TestEqual(TEXT("Unextracted runes are lost"), State->Profile->BankRunes, 0);
            Test->TestEqual(TEXT("Bank gold unaffected by death"), State->Profile->BankGold, 30);
            Player->ReturnOrRetry();
            Test->TestEqual(TEXT("Permanent health survives death, armor does not"), Player->Health->MaxHealth, 130.f);
            Test->TestEqual(TEXT("Permanent damage survives death"), Player->TotalDamageBonus(), 6);
            Player->SelectClass(EARPlayerClass::Mage);
            State->AdjustLoadout(TEXT("OakStaff"), 1, Player);
            State->AdjustLoadout(TEXT("Potion"), 1, Player);
            Test->TestTrue(TEXT("Second class deploys saved staff"), Mode->StartDungeon(Player));
            Mode->ClearRunActors();
            Test->TestEqual(TEXT("Mage has permanent and staff damage"), Player->TotalDamageBonus(), 14);
            State->BeginExtraction();
            Test->TestTrue(TEXT("Surviving carried items return on extraction"), State->SettleRun(true, Player->Inventory));
            Test->TestFalse(TEXT("Duplicate settlement cannot duplicate stash"), State->SettleRun(true, Player->Inventory));
            Test->TestEqual(TEXT("Staff returns exactly once"), State->Profile->Stash.FindRef(TEXT("OakStaff")), 1);
            Test->TestEqual(TEXT("Potion returns exactly once"), State->Profile->Stash.FindRef(TEXT("Potion")), 3);
            Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestTrue(TEXT("Complete loop persists talents and exact stash"), Saved && Saved->Stash.FindRef(TEXT("OakStaff")) == 1 && Saved->Stash.FindRef(TEXT("IronSword")) == 1 && Saved->TalentRanks.FindRef(TEXT("Vitality")) == 3);
            Player->ReturnOrRetry();
            Player->ToggleCamp();
            State->AdjustLoadout(TEXT("OakStaff"), 1, Player);
            Stage = 1;
#if WITH_EDITOR
            if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) GShaderCompilingManager->FinishAllCompilation();
#endif
            return false;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        {
            if (++CaptureFrames < 4) return false;
            if (!bCaptureRequested)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/M4_Camp.png")), true, false);
                bCaptureRequested = true;
                return false;
            }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
        }
        UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
        return true;
    }
};
bool FARCampTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARCampCommand(this));
    return true;
}
#endif
