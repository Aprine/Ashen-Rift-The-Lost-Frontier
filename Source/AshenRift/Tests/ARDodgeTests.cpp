#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "../ARBranchWing.h"
#include "../ARCombatFeedback.h"
#include "../ARGameWidget.h"
#include "../ARHealthComponent.h"
#include "../ARProgression.h"
#include "../ARPrototype.h"
#include "../ARRunGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDodgeTest, "AshenRift.Integration.Dodge",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
    int32 DodgeTestRoom(EARBranchRoom Room) { return static_cast<int32>(Room); }

    enum class EDodgeStep
    {
        Town, Arrive, ReadInput, Paused, ReturnVulnerability, CheckWindup,
        Wall, CheckWall, BossReady, Telegraph, Evade, BossImpact, Portal, LeavePortal, BackAtTown
    };

    class FARDodgeCommand : public IAutomationLatentCommand
    {
        FAutomationTestBase& Test;
        EDodgeStep Step = EDodgeStep::Town;
        bool bMage = false;
        int32 BossMove = 0;
        float HealthBefore = 0.f;
        float FrozenCooldown = 0.f;
        float ImpactAt = 0.f;
        double ResumeAt = 0.;
        double InputDeadline = 0.;
        const double Deadline = FPlatformTime::Seconds() + 80.;
        FVector Start;
        FVector FrozenPosition;
        FVector WallDirection;
        TWeakObjectPtr<AAREnemy> WindupTarget;
        TWeakObjectPtr<AAREnemy> Elite;
        TWeakObjectPtr<AARBoss> Warden;
        TWeakObjectPtr<AARExpeditionBranchWing> Wing;
        const FVector StandingOffset = FVector(0, 0, 105);

        bool Advance(EDodgeStep Next)
        {
            Step = Next;
            return false;
        }

        void PlacePlayer(AARPlayerCharacter* Player, APlayerController* PC, FVector Location, float Yaw)
        {
            Player->SetActorLocationAndRotation(Location, FRotator(0, Yaw, 0));
            PC->SetControlRotation(FRotator(0, Yaw, 0));
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Player->ConsumeMovementInputVector();
        }

        bool HasPlayerSpell(UWorld* World, AARPlayerCharacter* Player) const
        {
            for (TActorIterator<AARSpell> It(World); It; ++It)
                if (It->GetOwner() == Player) return true;
            return false;
        }

    public:
        explicit FARDodgeCommand(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Update() override
        {
            if (FPlatformTime::Seconds() > Deadline)
            {
                Test.AddError(FString::Printf(TEXT("Dodge timed out at step %d (mage=%d, boss move=%d)"),
                    static_cast<int32>(Step), bMage, BossMove));
                return true;
            }
            UWorld* World = nullptr;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::Game) World = Context.World();
            auto* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
            auto* Player = PC ? Cast<AARPlayerCharacter>(PC->GetPawn()) : nullptr;
            auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
            auto* UI = HUD ? HUD->GameUI : nullptr;
            auto* Mode = World ? World->GetAuthGameMode<AARPrototypeGameMode>() : nullptr;
            auto* State = World ? World->GetGameInstance<UARRunGameInstance>() : nullptr;
            if (!Player || !UI || !Mode || !State || !State->Profile ||
                !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_"))) return false;
            const float Now = World->GetTimeSeconds();
            const FKey Shift = bMage ? EKeys::RightShift : EKeys::LeftShift;
            switch (Step)
            {
            case EDodgeStep::Town:
                if (!Mode->IsExpeditionTown()) return false;
                Player->Dodge();
                if (!Test.TestTrue(TEXT("Dodging is restricted to expedition gameplay"),
                    !Player->IsDodging() && Player->DodgeCooldownRemaining() == 0.f &&
                    Player->SelectClass(bMage ? EARPlayerClass::Mage : EARPlayerClass::Warrior))) return true;
                if (!Test.TestTrue(TEXT("Depart normally from town to Random Ruins"),
                    Mode->DepartTown(Player, AARPrototypeGameMode::RandomRuinsDeparture))) return true;
                return Advance(EDodgeStep::Arrive);

            case EDodgeStep::Arrive:
            {
                if (!World->GetMapName().EndsWith(TEXT("Map_RandomRuins")) || !Player->CanPlay() || Now < 1.f) return false;
                for (TActorIterator<AARExpeditionBranchWing> It(World); It; ++It)
                    if (It->bStandalone && It->IsBuilt()) Wing = *It;
                if (!Wing.IsValid()) return false;
                for (TActorIterator<AAREnemy> It(World); It; ++It)
                {
                    if (auto* Boss = Cast<AARBoss>(*It)) { Warden = Boss; Boss->SetActorTickEnabled(false); }
                    else if (Wing->RoomAt(It->GetActorLocation()) == DodgeTestRoom(EARBranchRoom::Elite)) Elite = *It;
                    else It->Destroy();
                }
                if (!Test.TestTrue(TEXT("The real Ruins have an elite and a Warden"), Elite.IsValid() && Warden.IsValid())) return true;
                Start = Wing->Center(DodgeTestRoom(EARBranchRoom::Entry)) + StandingOffset;
                PlacePlayer(Player, PC, Start, 90);
                WindupTarget = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), Start + FVector(0, 155, 0), FRotator(0, -90, 0));
                if (!Test.TestTrue(TEXT("An attack target is in reach"), WindupTarget.IsValid())) return true;
                WindupTarget->SetActorTickEnabled(false);
                Player->Attack();
                if (!Test.TestTrue(TEXT("Normal class attack starts its windup"),
                    Player->Feedback->LastAction == (bMage ? EARCombatAction::Cast : EARCombatAction::Slash))) return true;
                PC->InputKey(EKeys::D, IE_Pressed, 1.f, false);
                PC->InputKey(Shift, IE_Pressed, 1.f, false);
                InputDeadline = FPlatformTime::Seconds() + 1.;
                return Advance(EDodgeStep::ReadInput);
            }

            case EDodgeStep::ReadInput:
                if (!Player->IsDodging() && FPlatformTime::Seconds() < InputDeadline) return false;
                PC->InputKey(EKeys::D, IE_Released, 0.f, false);
                PC->InputKey(Shift, IE_Released, 0.f, false);
                if (!Test.TestTrue(TEXT("Both physical Shift bindings start a dodge and refresh its HUD"),
                    Player->IsDodging() && UI->LabelText(TEXT("HudDodge")).Contains(TEXT("闪避中")))) return true;
                HealthBefore = Player->Health->CurrentHealth;
                UGameplayStatics::ApplyDamage(Player, 8.f, nullptr, nullptr, nullptr);
                if (!Test.TestEqual(TEXT("A hit during the short immunity window deals zero damage"),
                    Player->Health->CurrentHealth, HealthBefore)) return true;
                FrozenPosition = Player->GetActorLocation();
                FrozenCooldown = Player->DodgeCooldownRemaining();
                UI->TogglePause();
                ResumeAt = FPlatformTime::Seconds() + .16;
                return Advance(EDodgeStep::Paused);

            case EDodgeStep::Paused:
                if (FPlatformTime::Seconds() < ResumeAt) return false;
                Player->Health->ReceiveDamage(8.f);
                if (!Test.TestTrue(TEXT("Pause freezes travel, cooldown and the immunity window"),
                    Player->IsDodging() && Player->GetActorLocation().Equals(FrozenPosition, .1f) &&
                    FMath::IsNearlyEqual(Player->DodgeCooldownRemaining(), FrozenCooldown, .01f) &&
                    Player->Health->CurrentHealth == HealthBefore)) return true;
                UI->ExecuteAction(TEXT("Resume"));
                return Advance(EDodgeStep::ReturnVulnerability);

            case EDodgeStep::ReturnVulnerability:
            {
                if (Player->IsDodging()) return false;
                const FVector Moved = Player->GetActorLocation() - Start;
                if (!Test.TestTrue(TEXT("D and Shift together dodge camera-right, with a bounded distance"),
                    -Moved.X >= 340 && -Moved.X <= 372 && FMath::Abs(Moved.Y) <= 20)) return true;
                Player->Dodge();
                if (!Test.TestTrue(TEXT("Cooldown rejects another dodge and the HUD shows recovery"),
                    !Player->IsDodging() && UI->LabelText(TEXT("HudDodge")).Contains(TEXT("冷却")))) return true;
                PlacePlayer(Player, PC, Start, 90);
                return Advance(EDodgeStep::CheckWindup);
            }

            case EDodgeStep::CheckWindup:
                if (Player->DodgeCooldownRemaining() > 0.f) return false;
                if (!Test.TestTrue(TEXT("A dodge cancels the previous melee or cast without damage, projectile or mana cost"),
                    WindupTarget.IsValid() && WindupTarget->Health->CurrentHealth == WindupTarget->Health->MaxHealth &&
                    !HasPlayerSpell(World, Player) && Player->Mana == 100.f)) return true;
                Player->Health->ReceiveDamage(6.f);
                if (!Test.TestEqual(TEXT("The player becomes vulnerable after the dodge"), Player->Health->CurrentHealth, HealthBefore - 6.f)) return true;
                Player->Health->Heal(6.f);
                WindupTarget->Destroy();
                return Advance(EDodgeStep::Wall);

            case EDodgeStep::Wall:
            {
                const auto& Plan = Wing->Plan();
                const FIntPoint EntryToGuard = Plan.Cells[DodgeTestRoom(EARBranchRoom::Guard)] - Plan.Cells[DodgeTestRoom(EARBranchRoom::Entry)];
                WallDirection = FVector(-EntryToGuard.X, -EntryToGuard.Y, 0);
                Start = Wing->Center(DodgeTestRoom(EARBranchRoom::Entry)) + StandingOffset + WallDirection * 840.f;
                PlacePlayer(Player, PC, Start, WallDirection.Rotation().Yaw);
                Player->Dodge();
                if (!Test.TestTrue(TEXT("The no-input fallback dodges in the character's facing direction"), Player->IsDodging())) return true;
                return Advance(EDodgeStep::CheckWall);
            }

            case EDodgeStep::CheckWall:
            {
                if (Player->IsDodging()) return false;
                const float Travel = FVector::DotProduct(Player->GetActorLocation() - Start, WallDirection);
                if (!Test.TestTrue(TEXT("The dodge sweeps against an actual generated exterior wall"),
                    Travel > 20 && Travel < 140 && Wing->RoomAt(Player->GetActorLocation()) == DodgeTestRoom(EARBranchRoom::Entry))) return true;
                BossMove = 0;
                return Advance(EDodgeStep::BossReady);
            }

            case EDodgeStep::BossReady:
            {
                if (Player->DodgeCooldownRemaining() > 0.f) return false;
                FVector Near = Warden->GetActorLocation() - FVector(170, 0, 0);
                Near.Z = Wing->Center(DodgeTestRoom(EARBranchRoom::Boss)).Z + StandingOffset.Z;
                PlacePlayer(Player, PC, Near, 90);
                HealthBefore = Player->Health->CurrentHealth;
                Warden->SetActorTickEnabled(true);
                return Advance(EDodgeStep::Telegraph);
            }

            case EDodgeStep::Telegraph:
            {
                const EARCombatAction Expected[] = { EARCombatAction::BossArea, EARCombatAction::BossMelee, EARCombatAction::BossElbow };
                if (!(Warden->bCasting || Warden->bWindingAttack)) return false;
                if (!Test.TestEqual(TEXT("The Warden starts the expected ground slam, sweep or elbow"),
                    static_cast<uint8>(Warden->Feedback->LastAction), static_cast<uint8>(Expected[BossMove]))) return true;
                ImpactAt = Now + (BossMove == 0 ? 1.2f : .65f);
                return Advance(EDodgeStep::Evade);
            }

            case EDodgeStep::Evade:
                if (Now < ImpactAt - .085f) return false;
                Player->Dodge();
                if (!Test.TestTrue(TEXT("The player can dodge during a real Warden telegraph"), Player->IsDodging())) return true;
                return Advance(EDodgeStep::BossImpact);

            case EDodgeStep::BossImpact:
                if (Now < ImpactAt + .035f || Warden->bCasting || Warden->bWindingAttack) return false;
                if (!Test.TestEqual(TEXT("A timed dodge defeats the Warden's active hit"), Player->Health->CurrentHealth, HealthBefore)) return true;
                Warden->SetActorTickEnabled(false);
                ++BossMove;
                return Advance(BossMove < 3 ? EDodgeStep::BossReady : EDodgeStep::Portal);

            case EDodgeStep::Portal:
            {
                if (Player->DodgeCooldownRemaining() > 0.f) return false;
                if (bMage)
                {
                    Elite->Health->ReceiveDamage(5000);
                    if (!Test.TestFalse(TEXT("Defeating the real elite unlocks gold extraction"), Wing->DeepExtractionPortal()->IsSealed())) return true;
                }
                const auto PortalRoom = bMage ? EARBranchRoom::DeepExit : EARBranchRoom::Exit;
                PlacePlayer(Player, PC, Wing->Goal(DodgeTestRoom(PortalRoom)) + StandingOffset, 0);
                Player->Interact();
                if (!Test.TestTrue(TEXT("Begin blue or gold extraction normally"), State->Phase == EARRunPhase::Extracting)) return true;
                Player->Dodge();
                if (!Test.TestTrue(TEXT("A dodge can start from inside an active extraction"), Player->IsDodging())) return true;
                return Advance(EDodgeStep::LeavePortal);
            }

            case EDodgeStep::LeavePortal:
                if (Player->IsDodging()) return false;
                if (!Test.TestTrue(TEXT("Dodging out cancels the channel without settlement"),
                    State->Phase == EARRunPhase::InDungeon && Player->ExtractionRemaining() == 0.f &&
                    State->Summary.ExtractionBonusRunes == 0)) return true;
                Player->Health->ReceiveDamage(5000);
                if (!Test.TestTrue(TEXT("Death during dodge recovery settles the run normally"), State->Phase == EARRunPhase::Result)) return true;
                Player->ReturnOrRetry();
                return Advance(EDodgeStep::BackAtTown);

            case EDodgeStep::BackAtTown:
                if (!Mode->IsExpeditionTown()) return false;
                if (!Test.TestTrue(TEXT("The next pawn starts with a fresh dodge"), !Player->IsDodging() &&
                    Player->DodgeCooldownRemaining() == 0.f)) return true;
                if (bMage)
                {
                    UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
                    return true;
                }
                Wing.Reset();
                bMage = true;
                return Advance(EDodgeStep::Town);
            }
            return true;
        }
    };
}

bool FARDodgeTest::RunTest(const FString&)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")))
    {
        AddError(TEXT("Run from Map_Hub with -ARTestProfile."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FARDodgeCommand(*this));
    return true;
}
#endif
