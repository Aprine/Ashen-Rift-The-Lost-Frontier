#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "../ARGameWidget.h"
#include "../ARPrototype.h"
#include "../ARProgression.h"
#include "../AREnemyVariants.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../AREquipmentComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARUserInterfaceTest, "AshenRift.Integration.UserInterface",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
class FARUserInterfaceCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double Deadline = 0;
    float PausedWorldTime = 0, PausedSkill = 0;
    FVector PausedBoltLocation;
    TWeakObjectPtr<AAREnemyBolt> Bolt;
    FString CaptureName;
    int32 Frames = 0;
    bool bRequested = false;
    void Capture(const TCHAR* Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        {
#if WITH_EDITOR
            GShaderCompilingManager->FinishAllCompilation();
#endif
            CaptureName = Name; Frames = 0; bRequested = false;
        }
    }
    bool Click(UARGameWidget* UI, FName Action, int32 Payload = 0)
    {
        auto* Button = UI->FindAction(Action, Payload);
        if (!Button) { Test->AddError(FString(TEXT("Missing UI button: ")) + Action.ToString()); return false; }
        Button->OnClicked.Broadcast();
        return true;
    }
public:
    explicit FARUserInterfaceCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* P = World ? Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
        auto* S = World ? Cast<UARRunGameInstance>(World->GetGameInstance()) : nullptr;
        auto* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
        auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
        auto* UI = HUD ? HUD->GameUI : nullptr;
        if (!P || !S || !UI || !S->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Run L_RandomDungeon -game -ARTestProfile with UMG HUD.")); return true; }
        if (!CaptureName.IsEmpty())
        {
            if (++Frames < 4) return false;
            if (!bRequested) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false); bRequested = true; return false; }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            CaptureName.Empty(); return false;
        }
        auto* Mode = Cast<AARPrototypeGameMode>(World->GetAuthGameMode());
        switch (Stage)
        {
        case 0:
            Test->TestTrue(TEXT("Main menu can open safely in hub"), UI->OpenMainMenu());
            Test->TestTrue(TEXT("Main menu captures gameplay input and shows cursor"), UI->CurrentScreen() == EARUIScreen::MainMenu && P->IsUIBlockingInput() && PC->bShowMouseCursor);
            P->Interact();
            Test->TestTrue(TEXT("Gameplay E cannot bypass main menu"), S->Phase == EARRunPhase::Hub);
            Capture(TEXT("M6_MainMenu.png")); Stage = 1; return false;
        case 1:
        {
            if (!Click(UI, TEXT("Mage")) || !Click(UI, TEXT("Start"))) return true;
            Test->TestTrue(TEXT("Real button delegates select mage and open camp"), P->SelectedClass == EARPlayerClass::Mage && UI->CurrentScreen() == EARUIScreen::Camp);
            Test->TestFalse(TEXT("Wrong-screen purchase is rejected"), UI->ExecuteAction(TEXT("BuyPower")));
            Test->TestFalse(TEXT("Unowned loadout rejected through UI"), UI->ExecuteAction(TEXT("PreparePlus"), 1));
            if (!Click(UI, TEXT("Deploy"))) return true;
            Test->TestTrue(TEXT("Deployment returns focus to gameplay"), S->Phase == EARRunPhase::InDungeon && UI->CurrentScreen() == EARUIScreen::Game && !PC->bShowMouseCursor && !P->IsUIBlockingInput());
            Test->TestFalse(TEXT("Main menu cannot abandon active run"), UI->OpenMainMenu());
            Mode->ClearRunActors();
            P->Inventory->TryAdd(TEXT("OakStaff"), 1); P->Inventory->TryAdd(TEXT("LeatherArmor"), 1); P->Inventory->TryAdd(TEXT("Potion"), 2);
            P->Health->ReceiveDamage(20);
            P->bInventoryVisible = true; UI->Refresh();
            if (!Click(UI, TEXT("Equip"), 0) || !Click(UI, TEXT("Equip"), 1) || !Click(UI, TEXT("Potion"))) return true;
            Test->TestEqual(TEXT("Potion button invokes actual inventory consumption"), P->Inventory->Count(TEXT("Potion")), 1);
            Test->TestEqual(TEXT("Potion and armor update actual health"), P->Health->CurrentHealth, 85.f);
            Test->TestTrue(TEXT("Inventory labels are Chinese and reflect equipment"), UI->LabelText(TEXT("GearWeapon")).Contains(TEXT("橡木法杖")));
            auto* Target = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), P->GetActorLocation() + FVector(150, 0, 0), FRotator::ZeroRotator);
            if (!Target) { Test->AddError(TEXT("Input blocking target failed to spawn.")); return true; }
            const float Mana = P->Mana;
            P->Attack(); P->UseSkill();
            Test->TestEqual(TEXT("UI clicks cannot leak into attacks or skills"), P->Mana, Mana);
            Test->TestEqual(TEXT("Enemy remains unharmed while inventory captures input"), Target->Health->CurrentHealth, 60.f);
            Target->Destroy();
            Capture(TEXT("M6_Inventory.png")); Stage = 10; return false;
        }
        case 10:
        {
            if (!Click(UI, TEXT("Close"))) return true;
            P->SetActorLocation(FVector(7000, 0, 100));
            P->UseSkill();
            Test->TestTrue(TEXT("Closing inventory restores skill input"), P->SkillRemaining() > 5);
            auto* Owner = World->SpawnActor<AARRangedEnemy>(AARRangedEnemy::StaticClass(), FVector(8000, 700, 100), FRotator::ZeroRotator);
            if (!Owner) { Test->AddError(TEXT("Pause test caster failed to spawn.")); return true; }
            Owner->SetActorTickEnabled(false);
            const FTransform Transform(FRotator::ZeroRotator, FVector(7400, 500, 115));
            Bolt = World->SpawnActorDeferred<AAREnemyBolt>(AAREnemyBolt::StaticClass(), Transform, Owner, Owner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Bolt.IsValid()) { Test->AddError(TEXT("Pause test projectile failed to spawn.")); return true; }
            UGameplayStatics::FinishSpawningActor(Bolt.Get(), Transform);
            UI->TogglePause();
            Test->TestTrue(TEXT("Pause is a real world pause"), UGameplayStatics::IsGamePaused(World) && UI->CurrentScreen() == EARUIScreen::Pause);
            PausedWorldTime = World->GetTimeSeconds(); PausedSkill = P->SkillRemaining(); PausedBoltLocation = Bolt->GetActorLocation();
            Deadline = FPlatformTime::Seconds() + .3;
            Capture(TEXT("M6_Pause.png")); Stage = 2; return false;
        }
        case 2:
            if (FPlatformTime::Seconds() < Deadline) return false;
            Test->TestEqual(TEXT("World game time freezes while menu is paused"), World->GetTimeSeconds(), PausedWorldTime);
            Test->TestEqual(TEXT("Skill cooldown freezes while paused"), P->SkillRemaining(), PausedSkill);
            Test->TestTrue(TEXT("Projectile movement freezes while paused"), Bolt.IsValid() && Bolt->GetActorLocation().Equals(PausedBoltLocation, .01f));
            if (!Click(UI, TEXT("Resume"))) return true;
            Test->TestFalse(TEXT("Resume button unpauses the world"), UGameplayStatics::IsGamePaused(World));
            P->bInventoryVisible = true; UI->Refresh();
            Deadline = FPlatformTime::Seconds() + .2; Stage = 3; return false;
        case 3:
            if (FPlatformTime::Seconds() < Deadline) return false;
            Test->TestFalse(TEXT("Inventory deliberately does not pause combat"), UGameplayStatics::IsGamePaused(World));
            Test->TestTrue(TEXT("Projectile advances while inventory is open"), Bolt.IsValid() && FVector::Dist(Bolt->GetActorLocation(), PausedBoltLocation) > 20);
            Test->TestTrue(TEXT("Cooldown resumes after pause"), P->SkillRemaining() < PausedSkill);
            if (!Click(UI, TEXT("Close"))) return true;
            Mode->ClearRunActors();
            P->SetActorLocation(FVector(7000, 650, 100)); P->Interact(); UI->Refresh();
            Test->TestTrue(TEXT("Merchant interaction opens shop screen"), UI->CurrentScreen() == EARUIScreen::Shop);
            S->AddCurrency(30, 2);
            if (!Click(UI, TEXT("BuyPotion")) || !Click(UI, TEXT("BuyPower"))) return true;
            Test->TestEqual(TEXT("Shop buttons charge actual wallet once"), S->RunGold, 7);
            Test->TestEqual(TEXT("Shop potion reaches inventory"), P->Inventory->Count(TEXT("Potion")), 2);
            Test->TestEqual(TEXT("Shop power reaches actual player"), P->RunPowerBonus, 5);
            Capture(TEXT("M6_Shop.png")); Stage = 4; return false;
        case 4:
            if (!Click(UI, TEXT("Close"))) return true;
            S->BeginExtraction(); S->SettleRun(true, P->Inventory); UI->Refresh();
            Test->TestTrue(TEXT("Settlement screen displays Chinese outcome"), UI->CurrentScreen() == EARUIScreen::Result && UI->LabelText(TEXT("ResultTitle")) == TEXT("撤离成功"));
            Capture(TEXT("M6_Result.png")); Stage = 5; return false;
        case 5:
            if (!Click(UI, TEXT("Return"))) return true;
            P->ToggleCamp(); UI->Refresh();
            if (!Click(UI, TEXT("Vitality"))) return true;
            Test->TestEqual(TEXT("Talent button commits permanent rank"), S->TalentRank(TEXT("Vitality")), 1);
            Test->TestEqual(TEXT("Talent button spends banked rune once"), S->Profile->BankRunes, 1);
            if (!Click(UI, TEXT("PreparePlus"), 1)) return true;
            Test->TestEqual(TEXT("Loadout button prepares owned staff"), S->PreparedLoadout.FindRef(TEXT("OakStaff")), 1);
            Test->TestEqual(TEXT("Loadout selection does not debit stash"), S->Profile->Stash.FindRef(TEXT("OakStaff")), 1);
            if (!Click(UI, TEXT("Warrior"))) return true;
            Test->TestEqual(TEXT("Class button cancels incompatible loadout"), S->PreparedLoadout.FindRef(TEXT("OakStaff")), 0);
            if (!Click(UI, TEXT("Mage")) || !Click(UI, TEXT("PreparePlus"), 1)) return true;
            Capture(TEXT("M6_Camp.png")); Stage = 6; return false;
        case 6:
            Test->TestTrue(TEXT("Translated pickup names are Chinese"), UARGameWidget::TranslateNotice(TEXT("Collected 2 Potion")) == TEXT("获得 治疗药水 × 2"));
            if (!Click(UI, TEXT("Deploy"))) return true;
            Mode->ClearRunActors(); S->AddCurrency(9, 1); P->Health->ReceiveDamage(1000); UI->Refresh();
            Test->TestTrue(TEXT("Death presents localized loss screen"), UI->CurrentScreen() == EARUIScreen::Result && UI->LabelText(TEXT("ResultTitle")).Contains(TEXT("挑战失败")));
            Test->TestEqual(TEXT("Death UI preserves permanent bank"), S->Profile->BankGold, 7);
            Test->TestEqual(TEXT("Death UI does not refund withdrawn staff"), S->Profile->Stash.FindRef(TEXT("OakStaff")), 0);
            Capture(TEXT("M6_Death.png")); Stage = 7; return false;
        case 7:
            if (!Click(UI, TEXT("Return"))) return true;
            Test->TestTrue(TEXT("Death return restores game focus and living player"), UI->CurrentScreen() == EARUIScreen::Game && !PC->bShowMouseCursor && !P->Health->bDead);
            Test->TestEqual(TEXT("Permanent talent survives UI death loop"), P->Health->MaxHealth, 85.f);
            UGameplayStatics::DeleteGameInSlot(S->GetProfileSlot(), 0); return true;
        }
        return false;
    }
};
bool FARUserInterfaceTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARUserInterfaceCommand(this)); return true;
}
#endif
