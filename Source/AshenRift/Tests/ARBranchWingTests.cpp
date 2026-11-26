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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARBranchWingTest, "AshenRift.Integration.BranchWing",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

namespace
{
constexpr int32 Id(EARBranchRoom Room) { return static_cast<int32>(Room); }

class FARBranchWingCommand : public IAutomationLatentCommand
{
    FAutomationTestBase& Test;
    int32 Stage = 0;
    int32 Seed;
    float ReadyAt = 0;
    TArray<FVector> Retreat;
    int32 Step = 0;
    int32 ExpectedGold = 0;
    bool bCapturedMap = false;
    bool bCapturedRoom = false;
    FName Equipment;
    TWeakObjectPtr<AARPlayerCharacter> OriginalPawn;
    const double Deadline = FPlatformTime::Seconds() + 95;

    bool ValidateDoors(UWorld* World, AARExpeditionBranchWing* Wing, AARPlayerCharacter* Player)
    {
        const FVector Height(0, 0, 100);
        const auto& Plan = Wing->Plan();
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(ARBranchDoorTests), false, Player);
        const FCollisionShape Body = FCollisionShape::MakeCapsule(42, 96);
        bool bConnected = true;
        const FVector Exit = Wing->Goal(Id(EARBranchRoom::Exit));
        const FVector Return = Wing->Goal(Id(EARBranchRoom::Entry));
        for (int32 Room = 0; Room < Plan.Cells.Num(); ++Room)
        {
            for (const FVector& Goal : {Exit, Return})
            {
                const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                    World, Wing->Center(Room) + FVector(205, 120, 100), Goal + Height, Player);
                if (!Path || !Path->IsValid() || Path->IsPartial())
                {
                    Test.AddInfo(FString::Printf(TEXT("Room %d to %s"), Room, *Goal.ToCompactString()));
                    bConnected = false;
                }
            }
        }
        bool bCollisionMatches = true;
        const FIntPoint Sides[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
        for (int32 Room = 0; Room < Plan.Cells.Num(); ++Room)
        {
            for (const FIntPoint& Side : Sides)
            {
                const int32 Other = Plan.Cells.IndexOfByKey(Plan.Cells[Room] + Side);
                const FVector Direction(Side.X, Side.Y, 0);
                const FVector Border = Wing->Center(Room) + Height + Direction * (Wing->RoomSize / 2.f);
                FHitResult Hit;
                const bool bBlocked = World->SweepSingleByChannel(Hit, Border - Direction * 145.f, Border + Direction * 145.f,
                    FQuat::Identity, ECC_Pawn, Body, Query);
                if (bBlocked == Plan.Connects(Room, Other))
                {
                    bCollisionMatches = false;
                    Test.AddInfo(FString::Printf(TEXT("Door collision mismatch: room %d toward room %d"), Room, Other));
                }
            }
        }
        return Test.TestTrue(TEXT("Every room can reach the return passage and the branch exit"), bConnected) &
            Test.TestTrue(TEXT("A player-size capsule crosses only the edges shown on the map"), bCollisionMatches);
    }

public:
    FARBranchWingCommand(FAutomationTestBase& InTest, int32 InSeed) : Test(InTest), Seed(InSeed) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            Test.AddError(FString::Printf(TEXT("Branch test timed out at stage %d"), Stage));
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
        AARExpeditionRouteBoard* Board = nullptr;
        for (TActorIterator<AARExpeditionRouteBoard> It(World); It; ++It) Board = *It;
        auto* Wing = Board ? Board->BranchWing : nullptr;
        const FVector Stand(0, 0, 100);

        switch (Stage)
        {
        case 0:
            if (!Mode->IsExpeditionTown()) return false;
            Player->ToggleCamp();
            UI->Refresh();
            if (!Test.TestTrue(TEXT("Random departure starts the branch fixture"), UI->ExecuteAction(TEXT("DeployRandom")))) return true;
            Stage = 1;
            return false;
        case 1:
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon3")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < 1.15f) return false;
            if (!Test.TestTrue(TEXT("Dungeon 3 has a prepared plan before building any rooms"),
                IsValid(Wing) && Wing->IsPrepared() && !Wing->IsBuilt() && State->Seed == Seed)) return true;
            OriginalPawn = Player;
            Player->SetActorLocation(Board->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The route board can preview the generated graph"),
                UI->FindAction(TEXT("BranchMap")) && UI->ExecuteAction(TEXT("BranchMap")) &&
                UI->CurrentScreen() == EARUIScreen::BranchMap)) return true;
            for (int32 Room = 0; Room < Id(EARBranchRoom::Count); ++Room)
                Test.TestNotNull(TEXT("Every graph node can be selected"), UI->FindAction(TEXT("TrackBranchRoom"), Room));
            ReadyAt = World->GetTimeSeconds() + .4f;
            Stage = 2;
            return false;
        case 2:
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            if (!bCapturedMap && FParse::Param(FCommandLine::Get(), TEXT("ARCaptureBranchMap")))
            {
                if (World->GetTimeSeconds() < ReadyAt) return false;
                bCapturedMap = true;
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("V013_BranchMap.png")), true, false);
                return false;
            }
            if (!Test.TestTrue(TEXT("Selecting a node guides the player toward the entrance"),
                UI->ExecuteAction(TEXT("TrackBranchRoom"), Id(EARBranchRoom::Exit)) &&
                UI->LabelText(TEXT("HudRoute")).Contains(TEXT("随机支路入口")))) return true;
            State->AddCurrency(25, 0);
            Player->SetActorLocation(Wing->Entrance->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("E assembles the six-room wing and arrives on the entry module"),
                Wing->IsBuilt() && Wing->RoomAt(Player->GetActorLocation()) == Id(EARBranchRoom::Entry) && State->RunGold == 25)) return true;
            ReadyAt = World->GetTimeSeconds() + 1.1f;
            Stage = 3;
            return false;
        case 3:
        {
            if (World->GetTimeSeconds() < ReadyAt) return false;
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            if (!bCapturedRoom && FParse::Param(FCommandLine::Get(), TEXT("ARCaptureBranchMap")))
            {
                bCapturedRoom = true;
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("V013_BranchRoom.png")), true, false);
                return false;
            }
            int32 Modules = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
                if (It->ActorHasTag(TEXT("BranchModule"))) ++Modules;
            if (!Test.TestEqual(TEXT("Exactly six room modules were instantiated"), Modules, Id(EARBranchRoom::Count))) return true;
            if (!ValidateDoors(World, Wing, Player)) return true;
            Player->ToggleBranchMap();
            if (!Test.TestTrue(TEXT("M shows the current room in the wing"),
                UI->CurrentScreen() == EARUIScreen::BranchMap && UI->LabelText(TEXT("BranchNode0")).Contains(TEXT("●")))) return true;
            UI->ExecuteAction(TEXT("Close"));

            AARShop* Trader = nullptr;
            for (TActorIterator<AARShop> It(World); It; ++It)
                if (Wing->RoomAt(It->GetActorLocation()) == Id(EARBranchRoom::Supplies)) Trader = *It;
            if (!Test.TestNotNull(TEXT("The generated supply module contains a working trader"), Trader)) return true;
            Player->SetActorLocation(Trader->GetActorLocation() + FVector(0, 135, 45));
            Player->Interact();
            UI->Refresh();
            if (!Test.TestTrue(TEXT("The branch trader spends the same run wallet"),
                UI->ExecuteAction(TEXT("BuyPotion")) && State->RunGold == 17)) return true;
            UI->ExecuteAction(TEXT("Close"));

            AAREliteEnemy* Elite = nullptr;
            for (TActorIterator<AAREnemy> It(World); It; ++It)
            {
                const int32 Room = Wing->RoomAt(It->GetActorLocation());
                if (Room == Id(EARBranchRoom::Elite)) Elite = Cast<AAREliteEnemy>(*It);
                else if (Room != INDEX_NONE) It->Destroy();
            }
            Equipment = Wing->EliteReward();
            if (!Test.TestTrue(TEXT("The elite room contains the previewed equipment reward"), Elite && Elite->DropItemID == Equipment)) return true;
            ExpectedGold = State->RunGold + Elite->GoldReward;
            Elite->Health->ReceiveDamage(5000);
            AARLoot* Drop = nullptr;
            AARLoot* Gold = nullptr;
            for (TActorIterator<AARLoot> It(World); It; ++It)
            {
                if (Wing->RoomAt(It->GetActorLocation()) != Id(EARBranchRoom::Elite)) continue;
                if (It->ItemID == Equipment) Drop = *It;
                if (It->ItemID == TEXT("Gold")) Gold = *It;
            }
            if (!Test.TestNotNull(TEXT("The elite dropped actual loot in the room"), Drop)) return true;
            Player->SetActorLocation(Wing->Center(Id(EARBranchRoom::Elite)) + Stand);
            if (!Test.TestTrue(TEXT("The player can pick up the branching reward"), Gold && Gold->TryCollect(Player) && Drop->TryCollect(Player))) return true;
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                World, Player->GetActorLocation(), Wing->Goal(Id(EARBranchRoom::Entry)) + Stand, Player);
            if (!Test.TestTrue(TEXT("A retreat path exists from the elite room"), Path && Path->IsValid() && !Path->IsPartial())) return true;
            Retreat = Path->PathPoints;
            Player->GetCharacterMovement()->MaxWalkSpeed = 1050;
            Stage = 4;
            return false;
        }
        case 4:
            while (Retreat.IsValidIndex(Step) && FVector::DistSquared2D(Player->GetActorLocation(), Retreat[Step]) < FMath::Square(75.f)) ++Step;
            if (Retreat.IsValidIndex(Step))
            {
                Player->AddMovementInput((Retreat[Step] - Player->GetActorLocation()).GetSafeNormal2D());
                return false;
            }
            Player->Interact();
            if (!Test.TestTrue(TEXT("The player walked back through the linked modules and left with the same pawn and loot"),
                Wing->RoomAt(Player->GetActorLocation()) == INDEX_NONE && OriginalPawn.Get() == Player &&
                Player->Inventory->Count(Equipment) == 1 && State->RunGold == ExpectedGold)) return true;
            Player->SetActorLocation(Wing->Entrance->GetActorLocation() + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("Re-entry preserves the previously cleared branch"),
                Wing->RoomAt(Player->GetActorLocation()) == Id(EARBranchRoom::Entry))) return true;
            for (TActorIterator<AAREliteEnemy> It(World); It; ++It)
                Test.TestTrue(TEXT("Re-entry cannot respawn the branch elite"), Wing->RoomAt(It->GetActorLocation()) == INDEX_NONE);
            Player->SetActorLocation(Wing->Goal(Id(EARBranchRoom::Exit)) + Stand);
            Player->Interact();
            if (!Test.TestTrue(TEXT("The actual branch exit starts timed extraction"), State->Phase == EARRunPhase::Extracting)) return true;
            Stage = 5;
            return false;
        case 5:
            if (State->Phase != EARRunPhase::Result) return false;
            Test.TestTrue(TEXT("Branch extraction saves equipment, net gold and the tier unlock"),
                State->Summary.bSuccess && State->Profile->Stash.FindRef(Equipment) == 1 &&
                State->Profile->BankGold == ExpectedGold && State->HighestExpeditionTier() == 2);
            Player->ReturnOrRetry();
            Stage = 6;
            return false;
        case 6:
            if (!Mode->IsExpeditionTown()) return false;
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return true;
    }
};
}

bool FARBranchWingTest::RunTest(const FString& Parameters)
{
    int32 Seed = 0;
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("ARExpeditionSeed="), Seed) || ARExpedition::ChooseDungeon(Seed, 3) != 2)
    { AddError(TEXT("Use -ARTestProfile and a Dungeon 3 seed such as -ARExpeditionSeed=9.")); return false; }
    TSet<int32> Topologies;
    for (int32 Sample = 0; Sample < 128; ++Sample)
    {
        const auto Plan = FARBranchPlan::Generate(Sample);
        const auto Repeated = FARBranchPlan::Generate(Sample);
        TSet<FIntPoint> Cells;
        for (const FIntPoint& Cell : Plan.Cells) Cells.Add(Cell);
        bool bValid = Cells.Num() == Id(EARBranchRoom::Count) && Plan.Edges.Num() == Id(EARBranchRoom::Count) - 1;
        for (const FIntPoint& Edge : Plan.Edges)
        {
            const FIntPoint Delta = Plan.Cells[Edge.X] - Plan.Cells[Edge.Y];
            bValid &= FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) == 1;
        }
        if (!TestTrue(TEXT("Seed gives a repeatable plan with unique cells and adjacent doorways"),
            bValid && Plan.Cells == Repeated.Cells && Plan.Edges == Repeated.Edges)) return false;
        Topologies.Add((Plan.Connects(Id(EARBranchRoom::Guard), Id(EARBranchRoom::Supplies)) ? 1 : 0) |
            (Plan.Connects(Id(EARBranchRoom::Guard), Id(EARBranchRoom::Exit)) ? 2 : 0));
    }
    TestEqual(TEXT("Seeds change connections as well as orientation"), Topologies.Num(), 3);
    ADD_LATENT_AUTOMATION_COMMAND(FARBranchWingCommand(*this, Seed));
    return true;
}
#endif
