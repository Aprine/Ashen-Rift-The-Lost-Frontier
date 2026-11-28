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
#include "../AREnemyVariants.h"
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
    float DeepStarted = 0;
    bool bCaptured = false;
    TArray<FVector> Retreat;
    const double Deadline = FPlatformTime::Seconds() + 95;

    bool DefeatEncounter(UWorld* World, const AARExpeditionBranchWing* Wing, EARBranchRoom Encounter)
    {
        for (TActorIterator<AAREnemy> It(World); It; ++It)
        {
            if (Wing->RoomAt(It->GetActorLocation()) != Room(Encounter)) continue;
            It->Health->ReceiveDamage(5000);
            return true;
        }
        return false;
    }

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
        const FVector Boss = Wing->Center(Room(EARBranchRoom::Boss));
        for (const EARBranchRoom Neighbor : { EARBranchRoom::Elite, EARBranchRoom::DeepExit })
        {
            const FVector To = Wing->Center(Room(Neighbor));
            const FVector Direction = (To - Boss).GetSafeNormal2D();
            const FVector Door = (Boss + To) * .5f + Stand;
            FHitResult Hit;
            const bool bBlocked = World->SweepSingleByChannel(Hit, Door - Direction * 140, Door + Direction * 140,
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42, 96),
                FCollisionQueryParams(SCENE_QUERY_STAT(ARRuinsBossDoor), false, Player));
            if (!Test.TestFalse(TEXT("A player-size capsule can cross both boss room doorways"), bBlocked)) return false;
        }
        return true;
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
        const auto* Deep = Wing ? Wing->DeepExtractionPortal() : nullptr;
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
            const AARRangedEnemy* EntranceHexer = nullptr;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->ActorHasTag(TEXT("BranchModule"))) ++Modules;
                if (Cast<AARPortal>(*It)) ++Portals;
                if (Cast<AARBoss>(*It)) ++Bosses;
                if (const auto* Hexer = Cast<AARRangedEnemy>(*It))
                    if (Wing->RoomAt(Hexer->GetActorLocation()) == Room(EARBranchRoom::Guard)) EntranceHexer = Hexer;
            }
            if (!Test.TestTrue(TEXT("Independent map starts in a generated entry room with one boss and two exits"),
                Modules == Room(EARBranchRoom::Count) && Portals == 2 && Bosses == 1 &&
                IsValid(Deep) && Deep->IsSealed() && Deep->BonusRunes() == 2 &&
                Wing->RoomAt(Player->GetActorLocation()) == Room(EARBranchRoom::Entry) &&
                Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 1 && State->Seed == Seed &&
                State->ExpeditionTier == 1 && Player->Inventory->Count(TEXT("IronSword")) == 1 &&
                State->Profile->Stash.FindRef(TEXT("IronSword")) == 0)) return true;
            if (!Test.TestTrue(TEXT("Only the entrance Hexer funds early progression with one rune"),
                EntranceHexer && EntranceHexer->RuneReward == 1)) return true;
            if (!CheckNavigation(World, Player, Wing)) return true;
            const FVector Entry = Player->GetActorLocation();
            Player->SetActorLocation(Deep->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("A living elite blocks the gold portal and explains how to open it"),
                State->Phase == EARRunPhase::InDungeon && UI->LabelText(TEXT("HudContext")).Contains(TEXT("击败精英")))) return true;
            Player->SetActorLocation(Entry);
            Player->ToggleBranchMap();
            if (!Test.TestTrue(TEXT("M shows a trackable boss and gold portal with its sealed reward preview"),
                UI->CurrentScreen() == EARUIScreen::BranchMap && UI->FindAction(TEXT("TrackBranchRoom"), Room(EARBranchRoom::Boss)) &&
                UI->LabelText(TEXT("BranchEnemies")).Contains(TEXT("首领 1")) &&
                UI->FindAction(TEXT("TrackBranchRoom"), Room(EARBranchRoom::DeepExit)) &&
                UI->LabelText(TEXT("DeepExitStatus")).Contains(TEXT("额外 2 枚符文")))) return true;
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
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("V016_DeepExtractionMap.png")), true, false);
                return false;
            }
            if (!Test.TestTrue(TEXT("Tracking extraction closes the map and supplies walking guidance"),
                UI->ExecuteAction(TEXT("TrackBranchRoom"), Room(EARBranchRoom::DeepExit)) &&
                UI->LabelText(TEXT("HudRoute")).Contains(TEXT("深层撤离")))) return true;
            if (!Test.TestTrue(TEXT("The real elite defeat unseals the deep portal"),
                DefeatEncounter(World, Wing, EARBranchRoom::Elite) && !Deep->IsSealed())) return true;
            Player->SetActorLocation(Deep->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The gold portal channels for five seconds with no upfront reward"),
                State->Phase == EARRunPhase::Extracting && Player->ExtractionRemaining() > 4.9f && State->RunRunes == 0)) return true;
            Player->SetActorLocation(Wing->Goal(Room(EARBranchRoom::Exit)) + Stand);
            Stage = 3;
            return false;
        case 3:
            if (State->Phase == EARRunPhase::Extracting) return false;
            if (!Test.TestTrue(TEXT("Leaving the gold portal cancels the channel without awarding runes"),
                State->Phase == EARRunPhase::InDungeon && State->RunRunes == 0)) return true;
            Player->SetActorLocation(Deep->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The gold portal can start a new channel after leaving"),
                State->Phase == EARRunPhase::Extracting)) return true;
            Player->Health->ReceiveDamage(1);
            if (!Test.TestTrue(TEXT("Damage interrupts the gold portal without paying a bonus"),
                State->Phase == EARRunPhase::InDungeon && State->RunRunes == 0)) return true;
            Player->SetActorLocation(Wing->Goal(Room(EARBranchRoom::Exit)) + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The portal works while the optional boss is still alive"),
                State->Phase == EARRunPhase::Extracting && Player->ExtractionRemaining() <= 3.f &&
                Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 1)) return true;
            Stage = 4;
            return false;
        case 4:
            if (State->Phase != EARRunPhase::Result) return false;
            if (!Test.TestTrue(TEXT("Early extraction stores the carried item and unlocks tier two without a boss kill"),
                State->Summary.bSuccess && !State->Summary.bBossDefeated && State->Summary.ExtractionBonusRunes == 0 &&
                State->Profile->BankRunes == 0 && State->HighestExpeditionTier() == 2 &&
                State->Profile->Stash.FindRef(TEXT("IronSword")) == 1)) return true;
            Player->ReturnOrRetry();
            Stage = 5;
            return false;
        case 5:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The town previews the actual tier-two Ruins stats and portal reward"),
                UI->LabelText(TEXT("RuinsDifficulty")).Contains(TEXT("生命 +15%")) &&
                UI->LabelText(TEXT("RuinsDifficulty")).Contains(TEXT("伤害 +8%")) &&
                UI->LabelText(TEXT("RuinsDifficulty")).Contains(TEXT("额外 3")))) return true;
            if (!Test.TestTrue(TEXT("The next Ruins departure defaults to the newly unlocked tier"),
                State->SelectedExpeditionTier() == 2 && UI->ExecuteAction(TEXT("DeployRuins")))) return true;
            Stage = 6;
            return false;
        case 6:
        {
            if (!bInRuins) return false;
            AARBoss* Boss = nullptr;
            for (TActorIterator<AARBoss> It(World); It; ++It) Boss = *It;
            if (!Test.TestTrue(TEXT("Boss is in the end room and inherits tier two scaling"),
                Boss && Wing->RoomAt(Boss->GetActorLocation()) == Room(EARBranchRoom::Boss) &&
                Boss->DropItemID == Wing->BossReward() && State->ExpeditionTier == 2 && Deep->BonusRunes() == 3 &&
                FMath::IsNearlyEqual(Boss->Health->MaxHealth, Boss->GetClass()->GetDefaultObject<AARBoss>()->Health->MaxHealth * 1.15f) &&
                FMath::IsNearlyEqual(Boss->Damage, Boss->GetClass()->GetDefaultObject<AARBoss>()->Damage * 1.08f))) return true;
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
            Stage = 7;
            return false;
        }
        case 7:
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
            Stage = 8;
            return false;
        case 8:
            if (State->Phase != EARRunPhase::Result) return false;
            if (!Test.TestTrue(TEXT("Boss loot and tier three are banked once"),
                State->Summary.bSuccess && State->Summary.bBossDefeated && State->Profile->BankGold == BankedGold &&
                State->Profile->BankRunes == BankedRunes && State->HighestExpeditionTier() == 3 &&
                State->Profile->Stash.FindRef(AARExpeditionBranchWing::BossReward()) == 1)) return true;
            Player->ReturnOrRetry();
            Stage = 9;
            return false;
        case 9:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The Ruins can be entered again at tier three"),
                UI->ExecuteAction(TEXT("DeployRuins")) && State->PendingExpeditionTier == 3)) return true;
            Stage = 10;
            return false;
        case 10:
            if (!bInRuins) return false;
            if (!Test.TestTrue(TEXT("Elite death alone unseals the tier-three portal and its four-rune bonus"),
                DefeatEncounter(World, Wing, EARBranchRoom::Elite) && !Deep->IsSealed() && Deep->BonusRunes() == 4 &&
                DefeatEncounter(World, Wing, EARBranchRoom::DeepExit) && State->AddCurrency(0, 2))) return true;
            Player->SetActorLocation(Deep->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The deep exit works while the boss is alive"),
                State->Phase == EARRunPhase::Extracting && State->RunRunes == 2 &&
                Wing->LivingEnemies(Room(EARBranchRoom::Boss)) == 1)) return true;
            DeepStarted = World->GetTimeSeconds();
            Stage = 11;
            return false;
        case 11:
        {
            if (State->Phase != EARRunPhase::Result) return false;
            BankedRunes += 6;
            const auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            if (!Test.TestTrue(TEXT("The completed five-second gold channel saves collected and bonus runes exactly once"),
                World->GetTimeSeconds() - DeepStarted >= 4.99f && State->Summary.bSuccess && !State->Summary.bBossDefeated &&
                State->Summary.ExtractionBonusRunes == 4 && State->Summary.EarnedRunes == 2 &&
                UI->LabelText(TEXT("ResultBank")).Contains(TEXT("深层奖励 +4")) && Saved && Saved->BankRunes == BankedRunes &&
                State->Profile->BankRunes == BankedRunes && !State->SettleRun(true, Player->Inventory))) return true;
            Player->ReturnOrRetry();
            Stage = 12;
            return false;
        }
        case 12:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The returning player can depart again at tier three"),
                UI->ExecuteAction(TEXT("DeployRuins")) && State->PendingExpeditionTier == 3)) return true;
            Stage = 13;
            return false;
        case 13:
            if (!bInRuins) return false;
            if (!Test.TestTrue(TEXT("The gold portal is sealed each run and reopens when its elite falls"),
                Deep->IsSealed() && DefeatEncounter(World, Wing, EARBranchRoom::Elite) && !Deep->IsSealed())) return true;
            State->AddCurrency(75, 2);
            Player->SetActorLocation(Deep->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The fatal hit occurs during the deep channel"), State->Phase == EARRunPhase::Extracting)) return true;
            Player->Health->ReceiveDamage(5000);
            if (!Test.TestTrue(TEXT("Death loses the current Ruins rewards and preserves the bank and tier"),
                State->ExpeditionTier == 3 && State->Phase == EARRunPhase::Result && !State->Summary.bSuccess && State->Summary.LostGold == 75 &&
                State->Summary.ExtractionBonusRunes == 0 && State->Profile->BankGold == BankedGold && State->Profile->BankRunes == BankedRunes &&
                State->HighestExpeditionTier() == 3)) return true;
            Player->ReturnOrRetry();
            Stage = 14;
            return false;
        case 14:
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
        int32 BossDoors = 0, DeepDoors = 0;
        for (const FIntPoint& Edge : Plan.Edges)
        {
            if (Edge.X == Room(EARBranchRoom::Boss) || Edge.Y == Room(EARBranchRoom::Boss)) ++BossDoors;
            if (Edge.X == Room(EARBranchRoom::DeepExit) || Edge.Y == Room(EARBranchRoom::DeepExit)) ++DeepDoors;
        }
        const FIntPoint BossOffset = Plan.Cells[Room(EARBranchRoom::Boss)] - Plan.Cells[Room(EARBranchRoom::Elite)];
        const FIntPoint ExitOffset = Plan.Cells[Room(EARBranchRoom::DeepExit)] - Plan.Cells[Room(EARBranchRoom::Boss)];
        if (!TestTrue(TEXT("Every seed reproduces nine unique rooms with a deep exit connected only to the boss room"),
            Cells.Num() == Room(EARBranchRoom::Count) && BossDoors == 2 && DeepDoors == 1 &&
            FMath::Abs(BossOffset.X) + FMath::Abs(BossOffset.Y) == 1 && FMath::Abs(ExitOffset.X) + FMath::Abs(ExitOffset.Y) == 1 &&
            Plan.Connects(Room(EARBranchRoom::Elite), Room(EARBranchRoom::Boss)) &&
            Plan.Connects(Room(EARBranchRoom::Boss), Room(EARBranchRoom::DeepExit)) &&
            Plan.Cells == Repeated.Cells && Plan.Edges == Repeated.Edges)) return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FARRandomRuinsCommand(*this, Seed));
    return true;
}
#endif
