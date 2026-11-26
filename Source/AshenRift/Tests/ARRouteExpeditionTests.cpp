#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "../AREnemyVariants.h"
#include "../ARExpeditionSite.h"
#include "../ARGameWidget.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARProfile.h"
#include "../ARProgression.h"
#include "../ARPrototype.h"
#include "../ARRunGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARRouteExpeditionTest, "AshenRift.Integration.RouteExpeditions",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
class FARRouteExpeditionCommand : public IAutomationLatentCommand
{
    FAutomationTestBase& Test;
    int32 Stage = 0;
    const double Deadline = FPlatformTime::Seconds() + 70;

    bool CheckRoutes(UWorld* World, AARPlayerCharacter* Player, UARGameWidget* UI, UARRunGameInstance* State)
    {
        AARExpeditionRouteBoard* Board = nullptr;
        for (TActorIterator<AARExpeditionRouteBoard> It(World); It; ++It) Board = *It;
        if (!Test.TestNotNull(TEXT("Dungeon 3 has a route board"), Board)) return false;
        const FVector BoardApproach = Board->GetActorLocation() + FVector(0, -145, 100);
        const auto Reach = [&](const FVector& From, const FVector& To, const TCHAR* Label)
        {
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To, Player);
            if (!Path || !Path->IsValid() || Path->IsPartial())
                Test.AddInfo(FString::Printf(TEXT("%s -> %s"), *From.ToCompactString(), *To.ToCompactString()));
            return Test.TestTrue(Label, Path && Path->IsValid() && !Path->IsPartial());
        };
        bool bConnected = Reach(Player->GetActorLocation(), BoardApproach, TEXT("The route board is reachable from the entry"));
        bConnected &= Reach(BoardApproach, Board->Target(EARExpeditionRoute::Supplies), TEXT("The shop branch is reachable"));
        for (const FVector& Offset : Board->EliteOffsets)
            bConnected &= Reach(BoardApproach, Board->GetActorTransform().TransformPositionNoScale(Offset), TEXT("Every elite candidate is reachable"));
        for (const FVector& Offset : Board->ExtractionOffsets)
            bConnected &= Reach(BoardApproach, Board->GetActorTransform().TransformPositionNoScale(Offset), TEXT("Every alternate exit is reachable"));
        bConnected &= Reach(BoardApproach, Board->EntrancePortal->GetActorLocation(), TEXT("The original retreat is reachable"));
        if (!bConnected) return false;

        State->AddCurrency(12, 0);
        Player->Health->ReceiveDamage(15);
        Player->SetActorLocation(BoardApproach);
        Player->Interact();
        const bool bOpen = UI->CurrentScreen() == EARUIScreen::Routes &&
            UI->FindAction(TEXT("TrackRoute"), 1) && UI->FindAction(TEXT("TrackRoute"), 2) &&
            UI->FindAction(TEXT("TrackRoute"), 3) && UI->LabelText(TEXT("RouteStatus")).Contains(TEXT("12")) &&
            UI->LabelText(TEXT("RouteElite")).Contains(UARGameWidget::ItemName(Board->EliteReward()));
        if (!Test.TestTrue(TEXT("E opens three routes with live status, distances and the actual elite reward"), bOpen)) return false;
        Player->Health->ReceiveDamage(4);
        if (!Test.TestTrue(TEXT("Taking damage closes the route board"), UI->CurrentScreen() == EARUIScreen::Game)) return false;
        Player->Interact();
        if (!Test.TestTrue(TEXT("Supply choice sets guidance and keeps the player at the junction"),
            UI->ExecuteAction(TEXT("TrackRoute"), 1) && UI->LabelText(TEXT("HudRoute")).Contains(TEXT("补给路线")) &&
            FVector::Dist2D(BoardApproach, Player->GetActorLocation()) < 10)) return false;
        Player->Interact();
        if (!Test.TestTrue(TEXT("Elite choice starts its encounter"), UI->ExecuteAction(TEXT("TrackRoute"), 2) &&
            Board->HasStartedElite() && !Board->HasClearedElite())) return false;

        AAREliteEnemy* Elite = nullptr;
        for (TActorIterator<AAREliteEnemy> It(World); It; ++It) Elite = *It;
        if (!Test.TestTrue(TEXT("The live elite is at the planned node with the previewed drop"),
            Elite && Elite->DropItemID == Board->EliteReward() &&
            FVector::Dist2D(Elite->GetActorLocation(), Board->Target(EARExpeditionRoute::Elite)) < 100.f)) return false;
        Elite->Health->ReceiveDamage(5000);
        AARLoot* Reward = nullptr;
        for (TActorIterator<AARLoot> It(World); It; ++It)
            if (It->ItemID == Board->EliteReward()) Reward = *It;
        if (!Test.TestTrue(TEXT("Elite drops the promised equipment"), Reward && Reward->TryCollect(Player))) return false;
        Player->Interact();
        if (!Test.TestTrue(TEXT("Re-selecting a cleared branch does not respawn it"),
            UI->ExecuteAction(TEXT("TrackRoute"), 2) && Board->HasClearedElite())) return false;
        Player->Interact();
        if (!Test.TestTrue(TEXT("Extraction can be selected after the elite branch"), UI->ExecuteAction(TEXT("TrackRoute"), 3))) return false;

        const FVector Exit = Board->Target(EARExpeditionRoute::Extract);
        if (!Test.TestTrue(TEXT("The seeded shorter exit is a real alternate portal"),
            FVector::Dist2D(Exit, Board->EntrancePortal->GetActorLocation()) > 1000.f)) return false;
        World->GetAuthGameMode<AARPrototypeGameMode>()->ClearRunActors();
        Player->SetActorLocation(Exit + FVector(0, 0, 96));
        Player->Interact();
        return Test.TestTrue(TEXT("The alternate exit starts the usual timed extraction"), State->Phase == EARRunPhase::Extracting);
    }

public:
    explicit FARRouteExpeditionCommand(FAutomationTestBase& InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            Test.AddError(FString::Printf(TEXT("Route expedition timed out at stage %d"), Stage));
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
        if (!Player || !UI || !Mode || !State || !State->Profile || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_"))) return false;

        switch (Stage)
        {
        case 0:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("The first tier is selected and higher tiers cannot be chosen"),
                State->SelectedExpeditionTier() == 1 && !UI->ExecuteAction(TEXT("SelectTier"), 2))) return true;
            if (!Test.TestTrue(TEXT("Random departure carries the selected tier"), UI->ExecuteAction(TEXT("DeployRandom")) &&
                State->PendingExpeditionTier == 1 && !UI->ExecuteAction(TEXT("SelectTier"), 1))) return true;
            Stage = 1;
            return false;
        case 1:
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon3")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < 1.25f) return false;
            if (!Test.TestTrue(TEXT("The seeded Dungeon 3 arrived at tier 1"), State->Seed == 9 && State->ExpeditionTier == 1)) return true;
            if (!CheckRoutes(World, Player, UI, State)) return true;
            Stage = 2;
            return false;
        case 2:
        {
            if (State->Phase != EARRunPhase::Result) return false;
            const auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            if (!Test.TestTrue(TEXT("Successful extraction saves loot and unlocks and selects the next tier"),
                Saved && Saved->BankGold == 12 && Saved->Stash.FindRef(TEXT("RunicCharm")) == 1 &&
                Saved->HighestExpeditionTier == 2 && State->SelectedExpeditionTier() == 2 && State->Summary.UnlockedTier == 2)) return true;
            Player->ReturnOrRetry();
            Stage = 3;
            return false;
        }
        case 3:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("Unlocked tiers can be selected again"),
                UI->ExecuteAction(TEXT("SelectTier"), 1) && UI->ExecuteAction(TEXT("SelectTier"), 2) &&
                UI->ExecuteAction(TEXT("DeployRandom")))) return true;
            Stage = 4;
            return false;
        case 4:
        {
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon3")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < 1.25f) return false;
            int32 Elites = 0;
            bool bScaled = true;
            for (TActorIterator<AAREnemy> It(World); It; ++It)
            {
                const auto* Base = It->GetClass()->GetDefaultObject<AAREnemy>();
                bScaled &= FMath::IsNearlyEqual(It->Health->MaxHealth, Base->Health->MaxHealth * 1.2f) &&
                    FMath::IsNearlyEqual(It->Damage, Base->Damage * 1.1f) &&
                    It->GoldReward == FMath::RoundToInt(Base->GoldReward * 1.3f);
                if (Cast<AAREliteEnemy>(*It)) ++Elites;
            }
            Test.TestTrue(TEXT("Tier 2 has more elites and pays more against stronger enemies"),
                State->ExpeditionTier == 2 && Elites == 1 && bScaled);
            Player->Health->ReceiveDamage(5000);
            Test.TestTrue(TEXT("Death does not unlock or lock any tiers"),
                State->Phase == EARRunPhase::Result && State->HighestExpeditionTier() == 2 && State->Summary.UnlockedTier == 0);
            Player->ReturnOrRetry();
            Stage = 5;
            return false;
        }
        case 5:
            if (!Mode->IsExpeditionTown()) return false;
            for (int32 Tier : {1, 2, 3})
            {
                if (!Test.TestTrue(TEXT("The extracted tier can settle exactly once"),
                    State->BeginExpedition(9, Player->Inventory, Tier) && State->BeginExtraction() &&
                    State->SettleRun(true, Player->Inventory) && !State->SettleRun(true, Player->Inventory))) return true;
                Test.TestEqual(TEXT("Replays cannot skip tiers and the cap stays at 3"),
                    State->HighestExpeditionTier(), Tier == 1 ? 2 : 3);
                State->ReturnToHub();
            }
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return true;
    }
};
}

bool FARRouteExpeditionTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")))
    { AddError(TEXT("Use -ARTestProfile and -ARExpeditionSeed=9.")); return false; }
    TSet<int32> Maps;
    for (int32 Seed = 0; Seed < 32; ++Seed) Maps.Add(ARExpedition::ChooseDungeon(Seed, 3));
    TestEqual(TEXT("The seeded random departure can select all three maps"), Maps.Num(), 3);
    TestTrue(TEXT("A seed reproduces the encounter arrangement"),
        ARExpedition::MakeEncounters(5, 9, 3) == ARExpedition::MakeEncounters(5, 9, 3));
    ADD_LATENT_AUTOMATION_COMMAND(FARRouteExpeditionCommand(*this));
    return true;
}
#endif
