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
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "../ARBranchWing.h"
#include "../AREnemyVariants.h"
#include "../AREquipmentComponent.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARProfile.h"
#include "../ARProgression.h"
#include "../ARPrototype.h"
#include "../ARRunGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARRuinsBalanceSample, "AshenRift.Balance.Ruins",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
    int32 ToRoom(EARBranchRoom Room) { return static_cast<int32>(Room); }

    class FARRuinsSampleCommand : public IAutomationLatentCommand
    {
        FAutomationTestBase& Test;
        const bool bMage = FParse::Param(FCommandLine::Get(), TEXT("ARBalanceMage"));
        const bool bGold = FParse::Param(FCommandLine::Get(), TEXT("ARBalanceGold"));
        const int32 Tier;
        const int32 Seed;
        const double Deadline = FPlatformTime::Seconds() + 45.;
        const FVector Stand = FVector(0, 0, 105);
        TArray<EARBranchRoom> Route;
        TMultiMap<int32, TWeakObjectPtr<AAREnemy>> Encounters;
        TArray<FVector> Path;
        FVector PathGoal = FVector::ZeroVector;
        float PathAt = 0.f;
        int32 PathIndex = 0;
        int32 Leg = 0;
        int32 Potions = 0;
        float StartedAt = 0.f;
        float DamageTaken = 0.f;
        float LastHealth = 0.f;
        bool bDeparted = false;
        bool bStarted = false;

        void Walk(AARPlayerCharacter* Player, FVector Goal)
        {
            auto* World = Player->GetWorld();
            const float Now = World->GetTimeSeconds();
            if (Now >= PathAt || FVector::DistSquared2D(Goal, PathGoal) > FMath::Square(120.f))
            {
                PathAt = Now + .7f;
                PathGoal = Goal;
                Path.Reset();
                PathIndex = 1;
                const auto* Result = UNavigationSystemV1::FindPathToLocationSynchronously(World, Player->GetActorLocation(), Goal, Player);
                if (Result && Result->IsValid() && !Result->IsPartial()) Path = Result->PathPoints;
            }
            const FVector From = Player->GetActorLocation();
            while (Path.IsValidIndex(PathIndex) && FVector::DistSquared2D(From, Path[PathIndex]) < FMath::Square(90.f))
                ++PathIndex;
            const FVector Direction = ((Path.IsValidIndex(PathIndex) ? Path[PathIndex] : Goal) - From).GetSafeNormal2D();
            Player->AddMovementInput(Direction);
        }

        static bool HasLine(AARPlayerCharacter* Player, AAREnemy* Enemy)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ARRuinsSampleSight), false, Player);
            FHitResult Hit;
            return !Player->GetWorld()->LineTraceSingleByChannel(Hit, Player->GetActorLocation(),
                Enemy->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Enemy;
        }

        bool Fight(AARPlayerCharacter* Player, APlayerController* PC, AAREnemy* Enemy)
        {
            const FVector Offset = Enemy->GetActorLocation() - Player->GetActorLocation();
            if (Offset.Size() > (bMage ? 570.f : 195.f) || !HasLine(Player, Enemy)) return false;
            PC->SetControlRotation(FRotator(0, Offset.Rotation().Yaw, 0));
            Player->GetCharacterMovement()->StopMovementImmediately();
            if (Player->SkillRemaining() <= 0.f && (!bMage || Player->Mana >= 34.f))
                Player->UseSkill();
            Player->Attack();
            return true;
        }

        static void CollectRoom(UWorld* World, AARPlayerCharacter* Player, const AARExpeditionBranchWing* Wing, int32 Room)
        {
            for (TActorIterator<AARLoot> It(World); It; ++It)
                if (Wing->RoomAt(It->GetActorLocation()) == Room) It->TryCollect(Player);
        }

        void Record(UARRunGameInstance* State, float Seconds, const TCHAR* Outcome) const
        {
            int32 Gear = 0;
            if (State->Summary.bSuccess)
                for (const auto& Pair : State->Summary.Loot)
                    if (UAREquipmentComponent::IsEquipment(Pair.Key)) Gear += Pair.Value;
            UE_LOG(LogTemp, Display, TEXT("AR_RUINS_SAMPLE seed=%d class=%s tier=%d route=%s outcome=%s seconds=%.2f damage=%.1f potions=%d gold=%d runes=%d gear=%d"),
                Seed, bMage ? TEXT("Mage") : TEXT("Warrior"), Tier, bGold ? TEXT("Gold") : TEXT("Blue"),
                Outcome, Seconds, DamageTaken, Potions, State->Profile->BankGold, State->Profile->BankRunes, Gear);
        }

    public:
        FARRuinsSampleCommand(FAutomationTestBase& InTest, int32 InTier, int32 InSeed)
            : Test(InTest), Tier(InTier), Seed(InSeed) {}

        bool Update() override
        {
            if (FPlatformTime::Seconds() > Deadline)
            {
                Test.AddError(TEXT("Ruins balance sample exceeded the real-time watchdog."));
                return true;
            }
            UWorld* World = nullptr;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::Game) World = Context.World();
            auto* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
            auto* Player = PC ? Cast<AARPlayerCharacter>(PC->GetPawn()) : nullptr;
            auto* State = World ? World->GetGameInstance<UARRunGameInstance>() : nullptr;
            auto* Mode = World ? World->GetAuthGameMode<AARPrototypeGameMode>() : nullptr;
            if (!Player || !Mode || !State || !State->Profile || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_"))) return false;
            if (!bDeparted)
            {
                if (!Mode->IsExpeditionTown()) return false;
                State->Profile->HighestExpeditionTier = UARRunGameInstance::MaxExpeditionTier;
                if (!Test.TestTrue(TEXT("Start a fresh profile on the chosen class, seed and tier"),
                    State->SelectExpeditionTier(Tier) && Player->SelectClass(bMage ? EARPlayerClass::Mage : EARPlayerClass::Warrior) &&
                    Mode->DepartTown(Player, AARPrototypeGameMode::RandomRuinsDeparture))) return true;
                bDeparted = true;
                return false;
            }
            if (!World->GetMapName().EndsWith(TEXT("Map_RandomRuins"))) return false;
            AARExpeditionBranchWing* Wing = nullptr;
            for (TActorIterator<AARExpeditionBranchWing> It(World); It; ++It)
                if (It->bStandalone && It->IsBuilt()) Wing = *It;
            if (!Wing) return false;
            const float Now = World->GetTimeSeconds();
            if (!bStarted)
            {
                if (State->Phase != EARRunPhase::InDungeon || Now < .75f) return false;
                if (!Test.TestTrue(TEXT("The selected seed and tier are active"), State->Seed == Seed && State->ExpeditionTier == Tier)) return true;
                Route = { EARBranchRoom::Guard };
                if (bGold) Route.Append({ EARBranchRoom::Elite, EARBranchRoom::DeepExit });
                else Route.Add(EARBranchRoom::Exit);
                for (TActorIterator<AAREnemy> It(World); It; ++It)
                    Encounters.Add(Wing->RoomAt(It->GetActorLocation()), TWeakObjectPtr<AAREnemy>(*It));
                CollectRoom(World, Player, Wing, ToRoom(EARBranchRoom::Entry));
                StartedAt = Now;
                LastHealth = Player->Health->CurrentHealth;
                bStarted = true;
            }
            const float Health = Player->Health->CurrentHealth;
            DamageTaken += FMath::Max(0.f, LastHealth - Health);
            LastHealth = Health;
            if (State->Phase == EARRunPhase::Result || Now > StartedAt + 100.f)
            {
                const TCHAR* Outcome = State->Phase != EARRunPhase::Result ? TEXT("timeout") :
                    State->Summary.bSuccess ? TEXT("extracted") : TEXT("died");
                Record(State, Now - StartedAt, Outcome);
                UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
                return true;
            }
            if (State->Phase != EARRunPhase::InDungeon) return false;
            if (Health <= Player->Health->MaxHealth - 28.f && Player->Inventory->Count(TEXT("Potion")) > 0)
            {
                const int32 Before = Player->Inventory->Count(TEXT("Potion"));
                Player->UsePotion();
                Potions += Before - Player->Inventory->Count(TEXT("Potion"));
                LastHealth = Player->Health->CurrentHealth;
            }

            const EARBranchRoom GoalRoom = Route[Leg];
            const int32 Room = ToRoom(GoalRoom);
            if (Player->IsDodging()) return false;
            if (GoalRoom == EARBranchRoom::DeepExit && Wing->RoomAt(Player->GetActorLocation()) == ToRoom(EARBranchRoom::Boss))
            {
                Walk(Player, Wing->Goal(Room) + Stand);
                return false;
            }
            TArray<TWeakObjectPtr<AAREnemy>> Targets;
            Encounters.MultiFind(Room, Targets);
            AAREnemy* Nearest = nullptr;
            float Distance = MAX_flt;
            for (const auto& Weak : Targets)
                if (auto* Enemy = Weak.Get())
                {
                    const float Current = FVector::DistSquared(Player->GetActorLocation(), Enemy->GetActorLocation());
                    if (!Enemy->Health->bDead && Current < Distance) { Nearest = Enemy; Distance = Current; }
                }
            if (Nearest)
            {
                if (!Fight(Player, PC, Nearest))
                {
                    const FVector Towards = (Nearest->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
                    const FVector Approach = Nearest->GetActorLocation() - Towards * (bMage ? 380.f : 115.f);
                    Walk(Player, Distance < FMath::Square(720.f) && HasLine(Player, Nearest) ? Approach : Wing->Center(Room) + Stand);
                }
                return false;
            }

            CollectRoom(World, Player, Wing, Room);
            if (Leg < Route.Num() - 1)
            {
                ++Leg;
                return false;
            }
            const FVector Goal = Wing->Goal(Room);
            if (FVector::DistSquared2D(Player->GetActorLocation(), Goal) > FMath::Square(135.f))
                Walk(Player, Goal + Stand);
            else
            {
                Player->GetCharacterMovement()->StopMovementImmediately();
                Player->Interact();
            }
            return false;
        }
    };
}

bool FARRuinsBalanceSample::RunTest(const FString&)
{
    int32 Tier = 0, Seed = 0;
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("ARBalanceTier="), Tier) ||
        !FParse::Value(FCommandLine::Get(), TEXT("ARExpeditionSeed="), Seed) ||
        Tier < 1 || Tier > UARRunGameInstance::MaxExpeditionTier)
    {
        AddError(TEXT("Use Map_Hub, -ARTestProfile, -ARBalanceTier=1..3 and -ARExpeditionSeed."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FARRuinsSampleCommand(*this, Tier, Seed));
    return true;
}
#endif
