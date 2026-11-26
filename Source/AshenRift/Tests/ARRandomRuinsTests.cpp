#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"
#include "../ARBranchWing.h"
#include "../ARExpeditionSite.h"
#include "../ARGameWidget.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARProfile.h"
#include "../ARProgression.h"
#include "../ARPrototype.h"
#include "../ARRunGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARRandomRuinsTest, "AshenRift.Integration.RandomRuins",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
constexpr int32 Room(EARBranchRoom Value) { return static_cast<int32>(Value); }

class FARRandomRuinsCommand : public IAutomationLatentCommand
{
    FAutomationTestBase& Test;
    int32 Stage = 0;
    int32 Seed;
    int32 RetreatStep = 0;
    int32 BankedGold = 0;
    int32 BankedRunes = 0;
    float CaptureAt = 0;
    bool bCaptured = false;
    TArray<FVector> Retreat;
    const double Deadline = FPlatformTime::Seconds() + 95;

    bool CheckNavigation(UWorld* World, AARPlayerCharacter* Player, AARExpeditionBranchWing* Wing)
    {
        const FVector Stand(0, 0, 105);
        const FVector Exit = Wing->Goal(Room(EARBranchRoom::Exit)) + Stand;
        for (int32 Index = 0; Index < Wing->Plan().Cells.Num(); ++Index)
        {
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                World, Wing->Center(Index) + FVector(205, 120, 105), Exit, Player);
            if (!Path || !Path->IsValid() || Path->IsPartial())
            {
                Test.AddError(FString::Printf(TEXT("Room %d has no complete path to extraction"), Index));
                return false;
            }
        }
        const FVector From = Wing->Center(Room(EARBranchRoom::Elite));
        const FVector To = Wing->Center(Room(EARBranchRoom::Boss));
        const FVector Direction = (To - From).GetSafeNormal2D();
        const FVector Door = (From + To) * .5f + Stand;
        FHitResult Hit;
        const bool bBlocked = World->SweepSingleByChannel(Hit, Door - Direction * 140, Door + Direction * 140,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42, 96),
            FCollisionQueryParams(SCENE_QUERY_STAT(ARRuinsBossDoor), false, Player));
        return Test.TestFalse(TEXT("The player-size capsule can cross the new boss doorway"), bBlocked);
    }

public:
    FARRandomRuinsCommand(FAutomationTestBase& InTest, int32 InSeed) : Test(InTest), Seed(InSeed) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            Test.AddError(FString::Printf(TEXT("Random Ruins timed out at stage %d"), Stage));
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
        AARExpeditionBranchWing* Wing = nullptr;
        for (TActorIterator<AARExpeditionBranchWing> It(World); It; ++It)
            if (It->bStandalone) Wing = *It;
        const bool bInRuins = World->GetMapName().EndsWith(TEXT("Map_RandomRuins")) &&
            Wing && Wing->IsBuilt() && State->Phase == EARRunPhase::InDungeon && World->GetTimeSeconds() > 1.f;
        const FVector Stand(0, 0, 105);
        switch (Stage)
        {
        case 0:
            if (!Mode->IsExpeditionTown()) return false;
            State->Profile->Stash.Add(TEXT("IronSword"), 1);
            Player->ToggleCamp();
            UI->Refresh();
            if (!Test.TestTrue(TEXT("The town offers a separate Ruins button and prepares equipment"),
                UI->FindAction(TEXT("DeployRuins")) && State->AdjustLoadout(TEXT("IronSword"), 1, Player))) return true;
            if (!Test.TestTrue(TEXT("The new button queues Ruins travel before debiting gear"),
                UI->ExecuteAction(TEXT("DeployRuins")) && State->PendingExpeditionTier == 1 &&
                State->Profile->Stash.FindRef(TEXT("IronSword")) == 1)) return true;
            Stage = 1;
            return false;
        case 1:
        {
            if (!bInRuins) return false;
            int32 Modules = 0, Portals = 0, Bosses = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->ActorHasTag(TEXT("BranchModule"))) ++Modules;
                if (Cast<AARPortal>(*It)) ++Portals;
                if (Cast<AARBoss>(*It)) ++Bosses;
            }
            if (!Test.TestTrue(TEXT("Independent map starts in a generated entry room with one boss and one exit"),
                Modules == Room(EARBranchRoom::Count) && Portals == 1 && Bosses == 1 &&
                Wing->RoomAt(Player->GetActorLocation()) == Room(EARBranchRoom::Entry) &&
                Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 1 && State->Seed == Seed &&
                State->ExpeditionTier == 1 && Player->Inventory->Count(TEXT("IronSword")) == 1 &&
                State->Profile->Stash.FindRef(TEXT("IronSword")) == 0)) return true;
            if (!CheckNavigation(World, Player, Wing)) return true;
            Player->ToggleBranchMap();
            if (!Test.TestTrue(TEXT("M opens the standalone map with a trackable boss node"),
                UI->CurrentScreen() == EARUIScreen::BranchMap && UI->FindAction(TEXT("TrackBranchRoom"), Room(EARBranchRoom::Boss)) &&
                UI->LabelText(TEXT("BranchEnemies")).Contains(TEXT("首领 1")))) return true;
            CaptureAt = World->GetTimeSeconds() + .36f;
            Stage = 2;
            return false;
        }
        case 2:
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            if (!bCaptured && FParse::Param(FCommandLine::Get(), TEXT("ARCaptureRuins")))
            {
                if (World->GetTimeSeconds() < CaptureAt) return false;
                bCaptured = true;
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("V015_RuinsMap.png")), true, false);
                return false;
            }
            if (!Test.TestTrue(TEXT("Tracking extraction closes the map and supplies walking guidance"),
                UI->ExecuteAction(TEXT("TrackBranchRoom"), Room(EARBranchRoom::Exit)) &&
                UI->LabelText(TEXT("HudRoute")).Contains(TEXT("撤离")))) return true;
            Player->SetActorLocation(Wing->Goal(Room(EARBranchRoom::Exit)) + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The portal works while the optional boss is still alive"),
                State->Phase == EARRunPhase::Extracting && Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 1)) return true;
            Stage = 3;
            return false;
        case 3:
            if (State->Phase != EARRunPhase::Result) return false;
            if (!Test.TestTrue(TEXT("Early extraction stores the carried item and unlocks tier two without a boss kill"),
                State->Summary.bSuccess && !State->Summary.bBossDefeated && State->HighestExpeditionTier() == 2 &&
                State->Profile->Stash.FindRef(TEXT("IronSword")) == 1)) return true;
            Player->ReturnOrRetry();
            Stage = 4;
            return false;
        case 4:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The next Ruins departure defaults to the newly unlocked tier"),
                State->SelectedExpeditionTier() == 2 && UI->ExecuteAction(TEXT("DeployRuins")))) return true;
            Stage = 5;
            return false;
        case 5:
        {
            if (!bInRuins) return false;
            AARBoss* Boss = nullptr;
            for (TActorIterator<AARBoss> It(World); It; ++It) Boss = *It;
            if (!Test.TestTrue(TEXT("Boss is in the end room and inherits tier two scaling"),
                Boss && Wing->RoomAt(Boss->GetActorLocation()) == Room(EARBranchRoom::Boss) &&
                Boss->DropItemID == Wing->BossReward() && State->ExpeditionTier == 2 &&
                FMath::IsNearlyEqual(Boss->Health->MaxHealth, Boss->GetClass()->GetDefaultObject<AARBoss>()->Health->MaxHealth * 1.2f))) return true;
            const FVector RescueStart = Wing->Center(Room(EARBranchRoom::Boss)) + Stand;
            Boss->Health->ReceiveDamage(5000);
            if (!Test.TestTrue(TEXT("The real boss death records the victory and clears the map threat"),
                State->bBossDefeated && Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 0)) return true;
            for (TActorIterator<AAREnemy> It(World); It; ++It) It->Destroy();
            Player->SetActorLocation(RescueStart);
            for (TActorIterator<AARLoot> It(World); It; ++It)
                if (Wing->RoomAt(It->GetActorLocation()) == Room(EARBranchRoom::Boss)) It->TryCollect(Player);
            if (!Test.TestTrue(TEXT("Boss loot is real inventory and run currency"),
                Player->Inventory->Count(Wing->BossReward()) == 1 && State->RunGold > 0 && State->RunRunes > 0)) return true;
            BankedGold = State->RunGold;
            BankedRunes = State->RunRunes;
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                World, Player->GetActorLocation(), Wing->Goal(Room(EARBranchRoom::Exit)) + Stand, Player);
            if (!Test.TestTrue(TEXT("The boss room has a walkable retreat path"), Path && Path->IsValid() && !Path->IsPartial())) return true;
            Retreat = Path->PathPoints;
            Player->GetCharacterMovement()->MaxWalkSpeed = 1050;
            Stage = 6;
            return false;
        }
        case 6:
            while (Retreat.IsValidIndex(RetreatStep) && FVector::DistSquared2D(Player->GetActorLocation(), Retreat[RetreatStep]) < FMath::Square(72.f))
                ++RetreatStep;
            if (Retreat.IsValidIndex(RetreatStep))
            {
                Player->AddMovementInput((Retreat[RetreatStep] - Player->GetActorLocation()).GetSafeNormal2D());
                return false;
            }
            Player->Interact();
            if (!Test.TestTrue(TEXT("The player walks back from the boss room and begins extraction"),
                State->Phase == EARRunPhase::Extracting)) return true;
            Stage = 7;
            return false;
        case 7:
            if (State->Phase != EARRunPhase::Result) return false;
            if (!Test.TestTrue(TEXT("Boss loot and tier three are banked once"),
                State->Summary.bSuccess && State->Summary.bBossDefeated && State->Profile->BankGold == BankedGold &&
                State->Profile->BankRunes == BankedRunes && State->HighestExpeditionTier() == 3 &&
                State->Profile->Stash.FindRef(AARExpeditionBranchWing::BossReward()) == 1)) return true;
            Player->ReturnOrRetry();
            Stage = 8;
            return false;
        case 8:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The Ruins can be entered again at tier three"),
                UI->ExecuteAction(TEXT("DeployRuins")) && State->PendingExpeditionTier == 3)) return true;
            Stage = 9;
            return false;
        case 9:
            if (!bInRuins) return false;
            State->AddCurrency(75, 2);
            Player->Health->ReceiveDamage(5000);
            if (!Test.TestTrue(TEXT("Death loses the current Ruins rewards and preserves the bank and tier"),
                State->ExpeditionTier == 3 && State->Phase == EARRunPhase::Result && !State->Summary.bSuccess && State->Summary.LostGold == 75 &&
                State->Profile->BankGold == BankedGold && State->Profile->BankRunes == BankedRunes &&
                State->HighestExpeditionTier() == 3)) return true;
            Player->ReturnOrRetry();
            Stage = 10;
            return false;
        case 10:
            if (!Mode->IsExpeditionTown()) return false;
            Test.TestTrue(TEXT("The death result returns to the town start"), State->Phase == EARRunPhase::Hub && Player->IsAtCamp());
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return true;
    }
};
}

bool FARRandomRuinsTest::RunTest(const FString& Parameters)
{
    int32 Seed = 0;
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("ARExpeditionSeed="), Seed))
    { AddError(TEXT("Use -ARTestProfile and -ARExpeditionSeed=9.")); return false; }
    for (int32 Sample = 0; Sample < 128; ++Sample)
    {
        const auto Plan = FARBranchPlan::Generate(Sample, true);
        const auto Repeated = FARBranchPlan::Generate(Sample, true);
        TSet<FIntPoint> Cells;
        for (const FIntPoint& Cell : Plan.Cells) Cells.Add(Cell);
        int32 BossDoors = 0;
        for (const FIntPoint& Edge : Plan.Edges)
            if (Edge.X == Room(EARBranchRoom::Boss) || Edge.Y == Room(EARBranchRoom::Boss)) ++BossDoors;
        const FIntPoint Offset = Plan.Cells[Room(EARBranchRoom::Boss)] - Plan.Cells[Room(EARBranchRoom::Elite)];
        if (!TestTrue(TEXT("Every seed reproduces eight unique rooms with an optional adjacent end boss"),
            Cells.Num() == Room(EARBranchRoom::Count) && BossDoors == 1 && FMath::Abs(Offset.X) + FMath::Abs(Offset.Y) == 1 &&
            Plan.Connects(Room(EARBranchRoom::Elite), Room(EARBranchRoom::Boss)) &&
            Plan.Cells == Repeated.Cells && Plan.Edges == Repeated.Edges)) return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FARRandomRuinsCommand(*this, Seed));
    return true;
}
#endif
