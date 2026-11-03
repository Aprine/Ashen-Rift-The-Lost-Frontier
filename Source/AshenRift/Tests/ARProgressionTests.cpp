#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARProgressionTest, "AshenRift.Integration.Progression",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARProgressionCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    float WaitUntil = 0;
    float HealthBefore = 0;
    TWeakObjectPtr<AAREnemy> Target;
    TWeakObjectPtr<AAREnemy> Neighbor;
    TWeakObjectPtr<AARBoss> Boss;
    TWeakObjectPtr<AARShop> Shop;
    FString PendingCapture;
    int32 CaptureFrames = 0;
    bool bCaptureRequested = false;
    bool bMaterialsReady = false;
public:
    explicit FARProgressionCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    void Capture(const FString& Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        { PendingCapture = Name; CaptureFrames = 0; bCaptureRequested = false; }
    }
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        if (!World) { Test->AddError(TEXT("Start L_Progression with -game.")); return true; }
        auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
        auto* State = Cast<UARRunGameInstance>(World->GetGameInstance());
        if (!Player || !State || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Missing pawn or isolated test profile.")); return true; }
        const float Now = World->GetTimeSeconds();
        if (!bMaterialsReady && FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        {
            for (const TCHAR* Name : {TEXT("M_Arcane"), TEXT("M_Threat"), TEXT("M_Rune"), TEXT("M_Gold"), TEXT("M_Potion"), TEXT("M_Enemy"), TEXT("M_Warden")})
                LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/AshenRift/Materials/%s.%s"), Name, Name));
#if WITH_EDITOR
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
            bMaterialsReady = true;
            return false;
        }
        if (!PendingCapture.IsEmpty())
        {
            // Allow camera/HUD changes to reach a rendered frame before capture.
            if (++CaptureFrames < 3) return false;
            if (!bCaptureRequested)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), PendingCapture), true, false);
                bCaptureRequested = true;
                return false;
            }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            PendingCapture.Empty();
            return false;
        }
        if (Now < WaitUntil) return false;
        switch (Stage)
        {
        case 0:
        {
            Test->TestTrue(TEXT("Mage can be selected in camp"), Player->SelectClass(EARPlayerClass::Mage));
            Test->TestEqual(TEXT("Mage has distinct maximum health"), Player->Health->MaxHealth, 75.f);
            Player->Interact();
            Test->TestFalse(TEXT("Class switching blocked during dungeon"), Player->SelectClass(EARPlayerClass::Warrior));
            for (TActorIterator<AAREnemy> It(World); It; ++It)
            {
                if (auto* Warden = Cast<AARBoss>(*It)) Boss = Warden;
                else if (!Target.IsValid()) Target = *It;
            }
            for (TActorIterator<AARShop> It(World); It; ++It) Shop = *It;
            if (!Target.IsValid() || !Boss.IsValid() || !Shop.IsValid())
            { Test->AddError(TEXT("Progression map lacks enemy, boss or trader.")); return true; }
            // Shoot through a real room-divider wall; the projectile must stop.
            Player->SetActorLocation(FVector(1000, 600, 100));
            Target->SetActorLocation(FVector(1400, 600, 100));
            Player->Controller->SetControlRotation(FRotator::ZeroRotator);
            Player->Attack();
            Test->TestEqual(TEXT("Basic spell consumes 8 mana"), Player->Mana, 92.f);
            WaitUntil = Now + .7f;
            break;
        }
        case 1:
            Test->TestEqual(TEXT("Walls stop spells"), Target->Health->CurrentHealth, 60.f);
            Player->SetActorLocation(FVector(1800, 0, 100));
            Target->SetActorLocation(FVector(2400, 0, 100));
            Player->Controller->SetControlRotation(FRotator::ZeroRotator);
            Player->Attack();
            WaitUntil = Now + .7f;
            break;
        case 2:
        {
            Test->TestEqual(TEXT("Ranged projectile deals one hit"), Target->Health->CurrentHealth, 42.f);
            Neighbor = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), Target->GetActorLocation() + FVector(0, 130, 0), FRotator::ZeroRotator);
            const float ManaBefore = Player->Mana;
            Player->UseSkill();
            Test->TestEqual(TEXT("Arcane burst consumes 30 mana"), Player->Mana, ManaBefore - 30.f);
            Player->UseSkill();
            Test->TestEqual(TEXT("Cooldown prevents duplicate mana spend"), Player->Mana, ManaBefore - 30.f);
            Capture(TEXT("M2_Mage.png"));
            WaitUntil = Now + .7f;
            break;
        }
        case 3:
        {
            Test->TestFalse(TEXT("Burst kills the direct target"), Target.IsValid() && !Target->IsActorBeingDestroyed());
            if (Neighbor.IsValid()) Test->TestEqual(TEXT("Burst also damages nearby enemy exactly once"), Neighbor->Health->CurrentHealth, 15.f);
            else Test->AddError(TEXT("Burst neighbor unexpectedly died or failed to spawn."));
            for (TActorIterator<AARLoot> It(World); It; ++It) if (It->ItemID == TEXT("Gold")) It->TryCollect(Player);
            for (TActorIterator<AAREnemy> It(World); It; ++It) if (!Cast<AARBoss>(*It)) It->Destroy();
            Test->TestEqual(TEXT("First kill supplies 10 run gold"), State->RunGold, 10);
            Player->SetActorLocation(FVector(5050, 440, 100));
            Player->Interact();
            Test->TestTrue(TEXT("E opens trader"), Player->bShopOpen);
            Test->TestTrue(TEXT("Potion purchase succeeds"), Shop->Purchase(Player, 1));
            Test->TestEqual(TEXT("Potion costs exactly 8"), State->RunGold, 2);
            Test->TestFalse(TEXT("Insufficient gold cannot buy power"), Shop->Purchase(Player, 2));
            Test->TestEqual(TEXT("Failed purchase leaves power unchanged"), Player->RunPowerBonus, 0);
            State->AddCurrency(30, 0);
            Player->Inventory->MaxStack = 1;
            Test->TestFalse(TEXT("Full stack rejects purchase"), Shop->Purchase(Player, 1));
            Test->TestEqual(TEXT("Full stack purchase does not debit"), State->RunGold, 32);
            Player->Inventory->MaxStack = 99;
            for (int32 I = 0; I < 4; ++I) Test->TestTrue(TEXT("Remaining potion stock can be bought"), Shop->Purchase(Player, 1));
            State->AddCurrency(100, 0);
            Test->TestFalse(TEXT("Sold-out potion cannot be bought"), Shop->Purchase(Player, 1));
            Test->TestEqual(TEXT("Sold-out purchase does not debit"), State->RunGold, 100);
            for (int32 I = 0; I < 3; ++I) Test->TestTrue(TEXT("Power upgrades can be bought"), Shop->Purchase(Player, 2));
            Test->TestFalse(TEXT("Power upgrade stock limit enforced"), Shop->Purchase(Player, 2));
            Test->TestEqual(TEXT("Power capped at +15"), Player->RunPowerBonus, 15);
            Test->TestEqual(TEXT("Purchase accounting tracks total spend"), State->GoldSpent, 85);
            Test->TestEqual(TEXT("Purchases preserve bank"), State->Profile->BankGold, 0);
            Capture(TEXT("M2_Trader.png"));
            Stage = 30;
            WaitUntil = Now + .2f;
            return false;
        }
        case 30:
        {
            Player->SetActorLocation(FVector(5050, 0, 100));
            Test->TestFalse(TEXT("Remote purchase blocked"), Shop->Purchase(Player, 1));
            WaitUntil = Now + .15f;
            Stage = 4;
            return false;
        }
        case 4:
            Test->TestFalse(TEXT("Walking away closes shop"), Player->bShopOpen);
            Player->Health->ResetHealth();
            Player->SetActorLocation(FVector(6600, 0, 100));
            Boss->SetActorLocation(FVector(7400, 0, 150));
            HealthBefore = Player->Health->CurrentHealth;
            WaitUntil = Now + .2f;
            break;
        case 5:
        {
            bool bWarningExists = false;
            for (TActorIterator<AARBlastZone> It(World); It; ++It) if (It->GetOwner() == Boss.Get()) bWarningExists = true;
            Test->TestTrue(TEXT("Boss spawns a visible delayed attack zone"), bWarningExists);
            Capture(TEXT("M2_Boss.png"));
            Stage = 50;
            WaitUntil = Now + .15f;
            return false;
        }
        case 50:
        {
            Player->SetActorLocation(FVector(7100, 0, 100));
            WaitUntil = Now + 1.25f;
            Stage = 6;
            return false;
        }
        case 6:
        {
            Test->TestEqual(TEXT("Leaving telegraph avoids boss slam"), Player->Health->CurrentHealth, HealthBefore);
            Player->SetActorLocation(Boss->GetActorLocation() - FVector(900, 0, 0));
            const FVector Position(Player->GetActorLocation().X, Player->GetActorLocation().Y, 8);
            const FTransform Transform(Position);
            auto* Zone = World->SpawnActorDeferred<AARBlastZone>(AARBlastZone::StaticClass(), Transform, Boss.Get());
            if (!Zone) { Test->AddError(TEXT("Could not spawn delayed zone.")); return true; }
            Zone->Delay = .25f;
            UGameplayStatics::FinishSpawningActor(Zone, Transform);
            WaitUntil = Now + .35f;
            break;
        }
        case 7:
        {
            Test->TestEqual(TEXT("Staying in telegraph deals one delayed hit"), Player->Health->CurrentHealth, HealthBefore - 25.f);
            UGameplayStatics::ApplyDamage(Boss.Get(), 1000, Player->GetController(), Player, nullptr);
            Test->TestTrue(TEXT("Boss defeat recorded"), State->bBossDefeated);
            int32 GoldDrops = 0, RuneDrops = 0;
            for (TActorIterator<AARLoot> It(World); It; ++It)
            {
                if (It->ItemID == TEXT("Gold")) { ++GoldDrops; It->TryCollect(Player); }
                if (It->ItemID == TEXT("Rune")) { ++RuneDrops; It->TryCollect(Player); }
            }
            Test->TestEqual(TEXT("Boss grants one gold drop"), GoldDrops, 1);
            Test->TestEqual(TEXT("Boss grants one rune drop"), RuneDrops, 1);
            Test->TestEqual(TEXT("Boss rune amount"), State->RunRunes, 2);
            Test->TestEqual(TEXT("Remaining wallet includes purchases and boss loot"), State->RunGold, 125);
            Player->SetActorLocation(FVector(4800, -650, 100));
            Player->Interact();
            WaitUntil = Now + 3.2f;
            break;
        }
        case 8:
        {
            Test->TestTrue(TEXT("Boss route can extract"), State->Phase == EARRunPhase::Result && State->Summary.bSuccess);
            Test->TestTrue(TEXT("Boss defeat appears in summary"), State->Summary.bBossDefeated);
            Test->TestEqual(TEXT("Summary separates income from spending"), State->Summary.EarnedGold, 210);
            Test->TestEqual(TEXT("Summary records spending"), State->Summary.SpentGold, 85);
            Test->TestEqual(TEXT("Only remaining gold is banked"), State->Profile->BankGold, 125);
            Test->TestEqual(TEXT("Extracted runes are permanent"), State->Profile->BankRunes, 2);
            auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestNotNull(TEXT("Upgraded progression profile loads"), Saved);
            if (Saved) Test->TestEqual(TEXT("Runes persisted on disk"), Saved->BankRunes, 2);
            Player->ReturnOrRetry();
            Test->TestEqual(TEXT("Run power is cleared on return"), Player->RunPowerBonus, 0);
            Test->TestTrue(TEXT("Warrior can be selected next run"), Player->SelectClass(EARPlayerClass::Warrior));
            Test->TestEqual(TEXT("Warrior maximum health"), Player->Health->MaxHealth, 100.f);
            Player->Interact();
            Test->TestEqual(TEXT("Shop stock resets for next run"), Shop->PotionsSold, 0);
            Test->TestFalse(TEXT("Boss state resets for next run"), State->bBossDefeated);
            for (TActorIterator<AAREnemy> It(World); It; ++It) if (!Cast<AARBoss>(*It)) { Target = *It; break; }
            if (!Target.IsValid()) { Test->AddError(TEXT("Next run enemy missing")); return true; }
            Player->SetActorLocation(Target->GetActorLocation() - FVector(150, 0, 0));
            Player->Controller->SetControlRotation(FRotator::ZeroRotator);
            Player->UseSkill();
            Test->TestEqual(TEXT("Whirlwind hits nearby enemy"), Target->Health->CurrentHealth, 15.f);
            Player->UseSkill();
            Test->TestEqual(TEXT("Whirlwind cooldown blocks double damage"), Target->Health->CurrentHealth, 15.f);
            Player->Health->ReceiveDamage(1000);
            Test->TestEqual(TEXT("Death preserves previously banked gold"), State->Profile->BankGold, 125);
            Test->TestEqual(TEXT("Death preserves previously banked runes"), State->Profile->BankRunes, 2);
            Player->ReturnOrRetry();
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        }
        ++Stage;
        return false;
    }
};
bool FARProgressionTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARProgressionCommand(this));
    return true;
}
#endif
