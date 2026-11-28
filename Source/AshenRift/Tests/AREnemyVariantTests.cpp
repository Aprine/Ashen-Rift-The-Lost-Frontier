#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "../AREnemyVariants.h"
#include "../ARCombatFeedback.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
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
    TWeakObjectPtr<AAREnemy> Raider;
    TWeakObjectPtr<AAREliteEnemy> Elite;
    TWeakObjectPtr<AARBoss> Boss;
    TWeakObjectPtr<AARBlastZone> BossDeathZone;
    TWeakObjectPtr<AAREnemyBolt> Bolt;
    TWeakObjectPtr<AStaticMeshActor> Wall;
    FString CaptureName;
    int32 CaptureFrames = 0;
    bool bCaptureRequested = false;
    FVector ShieldReadyPosition;
    FVector StandingPelvis;
    bool IsPlayingClip(AAREnemy* Enemy, const TCHAR* Name)
    {
        auto* Anim = Enemy->GetMesh()->GetAnimInstance();
        auto* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
        if (!Montage || Montage->SlotAnimTracks.Num() == 0) return false;
        const auto& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
        return Segments.Num() && Segments[0].AnimReference && Segments[0].AnimReference->GetName() == Name;
    }
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
        const bool bPreview = UARFeedbackSubsystem::UsesUpdatedCombat();
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
            if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) Player->CameraBoom->SocketOffset.Y = 145.f;
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
            if (bPreview)
            {
                Test->TestTrue(TEXT("Hexer uses its cast animation"), IsPlayingClip(Ranged.Get(), TEXT("A_AR_HexerCast")));
                Ranged->Health->ReceiveDamage(1.f);
                Test->TestTrue(TEXT("Chip damage preserves the cast telegraph"), IsPlayingClip(Ranged.Get(), TEXT("A_AR_HexerCast")));
            }
            Deadline = Now + .32f;
            Stage = 12;
            return false;
        case 12:
            if (Now < Deadline) return false;
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
            if (bPreview)
            {
                Test->TestTrue(TEXT("Dead hexer plays the fall and loses collision"), Ranged.IsValid() &&
                    !Ranged->GetActorEnableCollision() && IsPlayingClip(Ranged.Get(), TEXT("A_AR_EnemyStaggerFall")));
                Player->SetActorLocation(FVector(7000, 0, 100));
                Raider = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), FVector(7150, 0, 100), FRotator(0, 180, 0));
                if (!Raider.IsValid()) { Test->AddError(TEXT("Raider failed to spawn.")); return true; }
                Raider->Health->ReceiveDamage(1.f);
                Test->TestTrue(TEXT("Raider recoils when hit outside its attack"), IsPlayingClip(Raider.Get(), TEXT("A_AR_EnemyRecoil")));
                Deadline = Now + 2.f; Stage = 50; return false;
            }
            Stage = 52;
            // Both runs continue with the existing sentinel checks.
        case 52:
            Player->SetActorLocation(FVector(7000, 0, 100));
            Elite = World->SpawnActor<AAREliteEnemy>(AAREliteEnemy::StaticClass(), FVector(7200, 0, 115), FRotator(0, 180, 0));
            if (!Elite.IsValid()) { Test->AddError(TEXT("Elite failed to spawn.")); return true; }
            Deadline = Now + 2;
            Stage = 6;
            return false;
        case 50:
            if (!Raider->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("Raider opens with an overhead chop"), Raider->bWindingAttack && IsPlayingClip(Raider.Get(), TEXT("A_AR_RaiderChop")));
            Deadline = Now + .24f; Stage = 51; return false;
        case 51:
            if (Now < Deadline) return false;
            Player->SetActorLocation(Raider->GetActorLocation() - FVector(300, 0, 0));
            Capture(TEXT("EnemyPreview_RaiderChop.png"));
            Deadline = Now + .28f; Stage = 55; return false;
        case 55:
            if (Now < Deadline) return false;
            Player->SetActorLocation(Raider->GetActorLocation() - FVector(140, 0, 0));
            Deadline = Now + 2.f; Stage = 56; return false;
        case 56:
            if (!Raider->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("Raider follows the chop with a different hook animation"), Raider->bWindingAttack && IsPlayingClip(Raider.Get(), TEXT("A_AR_RaiderSwing")));
            Raider->SetActorTickEnabled(false);
            Deadline = Now + .22f;
            Stage = 57; return false;
        case 57:
            if (Now < Deadline) return false;
            Capture(TEXT("EnemyPreview_RaiderHook.png"));
            Stage = 53; return false;
        case 53:
            StandingPelvis = Raider->GetMesh()->GetSocketLocation(TEXT("pelvis"));
            Raider->GoldReward = 0; // This target only verifies motion.
            Raider->Health->ReceiveDamage(1000);
            Test->TestTrue(TEXT("Raider switches from windup to its staged fall"), IsPlayingClip(Raider.Get(), TEXT("A_AR_EnemyStaggerFall")));
            Test->TestEqual(TEXT("The cancelled Raider windup does not damage the player"), Player->Health->CurrentHealth, 82.f);
            Deadline = Now + .49f; Stage = 54; return false;
        case 54:
            if (Now < Deadline) return false;
            Test->TestTrue(TEXT("Raider lowers its pelvis during the knee-buckle phase"), StandingPelvis.Z - Raider->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z > 10.f);
            Capture(TEXT("EnemyPreview_RaiderBuckle.png"));
            Deadline = Now + .48f; Stage = 58; return false;
        case 58:
            if (Now < Deadline) return false;
            Test->TestTrue(TEXT("Raider is still visible in the fall before despawn"), Raider.IsValid());
            Capture(TEXT("EnemyPreview_RaiderFall.png"));
            Stage = 52; return false;
        case 6:
            if (!Elite->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("Elite attack has visible windup"), Elite->bWindingAttack);
            if (bPreview)
            {
                Test->TestTrue(TEXT("Sentinel uses the guarded straight strike"), IsPlayingClip(Elite.Get(), TEXT("A_AR_SentinelShieldJab")));
                Test->TestTrue(TEXT("Shield is bound to the left forearm"),
                    Elite->Shield->GetAttachParent() == Elite->GetMesh() && Elite->Shield->GetAttachSocketName() == TEXT("lowerarm_l"));
                ShieldReadyPosition = Elite->Shield->GetComponentLocation();
            }
            Deadline = Now + .55f;
            Stage = 62;
            return false;
        case 62:
            if (Now < Deadline) return false;
            if (bPreview) Test->TestTrue(TEXT("Shield moves with the strike pose"), FVector::Dist(ShieldReadyPosition, Elite->Shield->GetComponentLocation()) > 2.f);
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
            // Death drops rewards once, whether the actor has a fall animation or despawns immediately.
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
            if (bPreview)
            {
                Boss = World->SpawnActor<AARBoss>(AARBoss::StaticClass(), FVector(7510, 0, 110), FRotator(0, 180, 0));
                if (!Boss.IsValid()) { Test->AddError(TEXT("Boss failed to spawn.")); return true; }
                Deadline = Now + 2.f;
                Stage = 80;
                return false;
            }
            Stage = 83;
            return false;
        }
        case 80:
            if (!Boss->bCasting && Now < Deadline) return false;
            Test->TestTrue(TEXT("The Warden's area warning plays the ground slam"), Boss->bCasting && IsPlayingClip(Boss.Get(), TEXT("A_AR_WardenGroundSlam")));
            Player->SetActorLocation(Boss->GetActorLocation() - FVector(165, 0, 20));
            Deadline = Now + .67f; Stage = 81; return false;
        case 81:
            if (Now < Deadline) return false;
            Capture(TEXT("BossPreview_Area.png"));
            Deadline = Now + 2.f; Stage = 82; return false;
        case 82:
            if (!Boss->bWindingAttack && Now < Deadline) return false;
            Test->TestTrue(TEXT("The Warden's close attack plays a separate sweep"), Boss->bWindingAttack && IsPlayingClip(Boss.Get(), TEXT("A_AR_WardenSweep")));
            Deadline = Now + .52f; Stage = 84; return false;
        case 84:
            if (Now < Deadline) return false;
            Capture(TEXT("BossPreview_Melee.png"));
            Deadline = Now + 2.4f; Stage = 85; return false;
        case 85:
            if (!IsPlayingClip(Boss.Get(), TEXT("A_AR_WardenElbow")) && Now < Deadline) return false;
            Test->TestTrue(TEXT("The Warden alternates into the left elbow strike"), Boss->bWindingAttack && IsPlayingClip(Boss.Get(), TEXT("A_AR_WardenElbow")));
            Deadline = Now + .38f; Stage = 86; return false;
        case 86:
            if (Now < Deadline) return false;
            Capture(TEXT("BossPreview_Elbow.png"));
            Stage = 87; return false;
        case 87:
        {
            const FTransform Transform(FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y, 8));
            auto* Zone = World->SpawnActorDeferred<AARBlastZone>(AARBlastZone::StaticClass(), Transform, Boss.Get());
            if (!Zone) { Test->AddError(TEXT("Boss death cancellation zone failed to spawn.")); return true; }
            BossDeathZone = Zone;
            UGameplayStatics::FinishSpawningActor(Zone, Transform);
            StandingPelvis = Boss->GetMesh()->GetSocketLocation(TEXT("pelvis"));
            Boss->Health->ReceiveDamage(1000.f);
            Test->TestTrue(TEXT("Defeating the Warden plays its own kneeling fall"),
                Boss.IsValid() && !Boss->GetActorEnableCollision() && IsPlayingClip(Boss.Get(), TEXT("A_AR_WardenKneelFall")));
            Test->TestTrue(TEXT("The boss-defeated flag is set immediately"), State->bBossDefeated);
            Player->SetActorLocation(Player->GetActorLocation() - FVector(145.f, 0.f, 0.f));
            Deadline = Now + .73f; Stage = 88; return false;
        }
        case 88:
            if (Now < Deadline) return false;
            Test->TestFalse(TEXT("The defeated Warden cannot leave an active blast zone"), BossDeathZone.IsValid());
            Test->TestTrue(TEXT("The Warden kneels before it collapses"),
                Boss.IsValid() && StandingPelvis.Z - Boss->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z > 25.f);
            Capture(TEXT("BossPreview_Kneel.png"));
            Deadline = Now + .78f; Stage = 89; return false;
        case 89:
            if (Now < Deadline) return false;
            if (Boss.IsValid()) Capture(TEXT("BossPreview_Fall.png"));
            for (TActorIterator<AARLoot> It(World); It; ++It)
                if ((It->ItemID == TEXT("Gold") && It->Quantity == 70) ||
                    (It->ItemID == TEXT("Rune") && It->Quantity == 2)) It->TryCollect(Player);
            Stage = 83; return false;
        case 83:
        {
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
            Test->TestEqual(TEXT("Extracted gold rewards saved"), State->Profile->BankGold, bPreview ? 90 : 20);
            Test->TestEqual(TEXT("Extracted rune rewards saved"), State->Profile->BankRunes, bPreview ? 3 : 1);
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
