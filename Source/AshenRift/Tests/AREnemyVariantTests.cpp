#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "../AREnemyVariants.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAREnemyVariantTest, "AshenRift.Integration.EnemyVariants",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FAREnemyVariantCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    float Deadline = 0;
    bool bSawBolt = false;
    TWeakObjectPtr<AARRangedEnemy> Ranged;
    TWeakObjectPtr<AAREliteEnemy> Elite;
    TWeakObjectPtr<AAREnemyBolt> Bolt;
    TWeakObjectPtr<AStaticMeshActor> Wall;
    FString CaptureName;
    int32 CaptureFrames = 0;
    bool bCaptureRequested = false;
    void Capture(const TCHAR* Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        {
#if WITH_EDITOR
            GShaderCompilingManager->FinishAllCompilation();
#endif
            CaptureName = Name; CaptureFrames = 0; bCaptureRequested = false;
        }
    }
    void ClearBolts(UWorld* World)
    { for (TActorIterator<AAREnemyBolt> It(World); It; ++It) It->Destroy(); }
    AAREnemyBolt* SpawnBolt(UWorld* World, AAREnemy* Owner, const FVector& Position)
    {
        const FTransform Transform(FRotator::ZeroRotator, Position);
        auto* Shot = World->SpawnActorDeferred<AAREnemyBolt>(AAREnemyBolt::StaticClass(), Transform, Owner, Owner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Shot) UGameplayStatics::FinishSpawningActor(Shot, Transform);
        return Shot;
    }
public:
    explicit FAREnemyVariantCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* Player = World ? Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
        auto* State = World ? Cast<UARRunGameInstance>(World->GetGameInstance()) : nullptr;
        if (!Player || !State || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Use L_RandomDungeon -game -ARTestProfile.")); return true; }
        auto* Mode = Cast<AARPrototypeGameMode>(World->GetAuthGameMode());
        const float Now = World->GetTimeSeconds();
        if (!CaptureName.IsEmpty())
        {
            if (++CaptureFrames < 3) return false;
            if (!bCaptureRequested)
            { FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false); bCaptureRequested = true; return false; }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            CaptureName.Empty();
            return false;
        }
        switch (Stage)
        {
        case 0:
        {
#if WITH_EDITOR
            if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) GShaderCompilingManager->FinishAllCompilation();
#endif
            if (!Mode || !Mode->StartDungeon(Player)) { Test->AddError(TEXT("Could not start dungeon.")); return true; }
            int32 MeleeCount = 0, RangedCount = 0, EliteCount = 0, BossCount = 0;
            for (TActorIterator<AAREnemy> It(World); It; ++It)
            {
                if (Cast<AARBoss>(*It)) ++BossCount;
                else if (Cast<AARRangedEnemy>(*It)) ++RangedCount;
                else if (Cast<AAREliteEnemy>(*It)) ++EliteCount;
                else ++MeleeCount;
            }
            Test->TestEqual(TEXT("Two ordinary melee enemies"), MeleeCount, 2);
            Test->TestEqual(TEXT("Three ranged enemies"), RangedCount, 3);
            Test->TestEqual(TEXT("One elite beyond extraction room"), EliteCount, 1);
            Test->TestEqual(TEXT("Boss still present"), BossCount, 1);
            Mode->ClearRunActors();
            Player->SetActorLocation(FVector(7000, 0, 100));
            Ranged = World->SpawnActor<AARRangedEnemy>(AARRangedEnemy::StaticClass(), FVector(7800, 0, 100), FRotator(0, 180, 0));
            if (!Ranged.IsValid()) { Test->AddError(TEXT("Ranged enemy failed to spawn.")); return true; }
            Deadline = Now + 2;
            Stage = 1;
            return false;
        }
        case 1:
            if (!Ranged->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("Ranged enemy telegraphs before firing"), Ranged->bWindingAttack);
            Test->TestEqual(TEXT("Windup cannot immediately deal damage"), Player->Health->CurrentHealth, 100.f);
            Capture(TEXT("M5_Hexer.png"));
            Stage = 11;
            return false;
        case 11:
            // The contributed supply-room counter is on +Y. Dodge into the
            // open side so this still tests evasion rather than cover cancelling
            // the caster's line of sight before a projectile is released.
            Player->SetActorLocation(FVector(7000, -450, 100));
            Deadline = Now + 1.9f;
            Stage = 2;
            return false;
        case 2:
            for (TActorIterator<AAREnemyBolt> It(World); It; ++It) bSawBolt = true;
            if (Now < Deadline) return false;
            Test->TestTrue(TEXT("AI actually spawns moving projectiles"), bSawBolt);
            Test->TestEqual(TEXT("Stepping sideways dodges locked aim"), Player->Health->CurrentHealth, 100.f);
            Ranged->Destroy();
            ClearBolts(World);
            Player->SetActorLocation(FVector(7000, 0, 100));
            Ranged = World->SpawnActor<AARRangedEnemy>(AARRangedEnemy::StaticClass(), FVector(7800, 0, 100), FRotator(0, 180, 0));
            Deadline = Now + 4;
            if (!Ranged.IsValid()) { Test->AddError(TEXT("Stationary-shot enemy failed to spawn.")); return true; }
            Stage = 3;
            return false;
        case 3:
            if (Player->Health->CurrentHealth == 100 && Now < Deadline) return false;
            Test->TestEqual(TEXT("Stationary player takes exactly one bolt hit"), Player->Health->CurrentHealth, 82.f);
            Ranged->SetActorTickEnabled(false);
            Ranged->SetActorLocation(FVector(6900, 500, 100));
            ClearBolts(World);
            Player->SetActorLocation(FVector(7800, 0, 100));
            Wall = World->SpawnActor<AStaticMeshActor>(FVector(7400, 0, 120), FRotator::ZeroRotator);
            if (!Wall.IsValid()) { Test->AddError(TEXT("Test cover failed to spawn.")); return true; }
            Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
            Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            Wall->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
            Wall->SetActorScale3D(FVector(.6f, 4, 3));
            Bolt = SpawnBolt(World, Ranged.Get(), FVector(7000, 0, 115));
            if (!Bolt.IsValid()) { Test->AddError(TEXT("Cover-test projectile failed to spawn.")); return true; }
            Deadline = Now + 1.3f;
            Stage = 4;
            return false;
        case 4:
            if (Now < Deadline) return false;
            Test->TestFalse(TEXT("Wall stops the hostile projectile"), Bolt.IsValid());
            Test->TestEqual(TEXT("Projectile cannot damage through cover"), Player->Health->CurrentHealth, 82.f);
            Wall->Destroy();
            Bolt = SpawnBolt(World, Ranged.Get(), FVector(7000, 0, 115));
            if (!Bolt.IsValid()) { Test->AddError(TEXT("Death-cancellation projectile failed to spawn.")); return true; }
            Ranged->Health->ReceiveDamage(1000);
            Deadline = Now + .2f;
            Stage = 5;
            return false;
        case 5:
            if (Now < Deadline) return false;
            Test->TestFalse(TEXT("Killing caster cancels its existing projectile"), Bolt.IsValid());
            Test->TestEqual(TEXT("Dead caster cannot hit the player"), Player->Health->CurrentHealth, 82.f);
            Player->SetActorLocation(FVector(7000, 0, 100));
            Elite = World->SpawnActor<AAREliteEnemy>(AAREliteEnemy::StaticClass(), FVector(7200, 0, 115), FRotator(0, 180, 0));
            if (!Elite.IsValid()) { Test->AddError(TEXT("Elite failed to spawn.")); return true; }
            Deadline = Now + 2;
            Stage = 6;
            return false;
        case 6:
            if (!Elite->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("Elite attack has visible windup"), Elite->bWindingAttack);
            Capture(TEXT("M5_Sentinel.png"));
            Stage = 61;
            return false;
        case 61:
            // Remain inside range but move behind the locked facing direction.
            Player->SetActorLocation(Elite->GetActorLocation() + FVector(150, 0, 0));
            Deadline = Now + 1.05f;
            Stage = 7;
            return false;
        case 7:
            if (Now < Deadline) return false;
            Test->TestEqual(TEXT("Elite cannot hit behind its committed swing"), Player->Health->CurrentHealth, 82.f);
            Elite->Destroy();
            Player->Health->Heal(100);
            Player->SetActorLocation(FVector(7000, 0, 100));
            Elite = World->SpawnActor<AAREliteEnemy>(AAREliteEnemy::StaticClass(), FVector(7200, 0, 115), FRotator(0, 180, 0));
            Deadline = Now + 3;
            if (!Elite.IsValid()) { Test->AddError(TEXT("Heavy-hit enemy failed to spawn.")); return true; }
            Stage = 8;
            return false;
        case 8:
        {
            if (Player->Health->CurrentHealth == 100 && Now < Deadline) return false;
            Test->TestEqual(TEXT("Elite's heavy attack deals twenty damage once"), Player->Health->CurrentHealth, 80.f);
            Elite->SetActorTickEnabled(false);
            Elite->DropItemID = TEXT("LeatherArmor");
            // Death destroys the actor and invalidates its weak pointer immediately.
            auto* DefeatedHealth = Elite->Health;
            DefeatedHealth->ReceiveDamage(1000);
            DefeatedHealth->ReceiveDamage(1000);
            int32 GoldDrops = 0, RuneDrops = 0, ArmorDrops = 0;
            for (TActorIterator<AARLoot> It(World); It; ++It)
            {
                if (It->ItemID == TEXT("Gold") && It->Quantity == 20) { ++GoldDrops; It->TryCollect(Player); }
                if (It->ItemID == TEXT("Rune")) { ++RuneDrops; Test->TestEqual(TEXT("Elite awards one rune"), It->Quantity, 1); It->TryCollect(Player); }
                if (It->ItemID == TEXT("LeatherArmor")) { ++ArmorDrops; It->TryCollect(Player); }
            }
            Test->TestEqual(TEXT("Elite gold reward spawned once"), GoldDrops, 1);
            Test->TestEqual(TEXT("Elite rune reward spawned once"), RuneDrops, 1);
            Test->TestEqual(TEXT("Elite equipment reward spawned once"), ArmorDrops, 1);
            Ranged = World->SpawnActor<AARRangedEnemy>(AARRangedEnemy::StaticClass(), FVector(8000, 500, 100), FRotator::ZeroRotator);
            if (!Ranged.IsValid()) { Test->AddError(TEXT("Settlement-test caster failed to spawn.")); return true; }
            Ranged->SetActorTickEnabled(false);
            Bolt = SpawnBolt(World, Ranged.Get(), FVector(7500, 500, 115));
            if (!Bolt.IsValid()) { Test->AddError(TEXT("Settlement-test projectile failed to spawn.")); return true; }
            State->BeginExtraction();
            Test->TestTrue(TEXT("Enemy rewards settle through existing extraction"), State->SettleRun(true, Player->Inventory));
            Deadline = Now + .2f;
            Stage = 9;
            return false;
        }
        case 9:
            if (Now < Deadline) return false;
            Test->TestFalse(TEXT("Settlement cancels remaining hostile projectiles"), Bolt.IsValid());
            Test->TestEqual(TEXT("Extracted elite gold saved"), State->Profile->BankGold, 20);
            Test->TestEqual(TEXT("Extracted elite rune saved"), State->Profile->BankRunes, 1);
            Test->TestEqual(TEXT("Extracted elite equipment saved"), State->Profile->Stash.FindRef(TEXT("LeatherArmor")), 1);
            Player->ReturnOrRetry();
            Test->TestTrue(TEXT("Run cleanup removes hostile projectiles"), !TActorIterator<AAREnemyBolt>(World));
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return false;
    }
};
bool FAREnemyVariantTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FAREnemyVariantCommand(this));
    return true;
}
#endif
