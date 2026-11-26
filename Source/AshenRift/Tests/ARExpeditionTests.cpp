#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "../ARPrototype.h"
#include "../ARProgression.h"
#include "../ARGameWidget.h"
#include "../ARExpeditionSite.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARExpeditionTest, "AshenRift.Integration.Expeditions",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARExpeditionCommand : public IAutomationLatentCommand
{
    FAutomationTestBase& Test;
    int32 Stage = 0;
    double Deadline = FPlatformTime::Seconds() + 75;
    FString CaptureName;
    int32 CaptureFrames = 0;

    void Capture(const TCHAR* Name)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) return;
#if WITH_EDITOR
        GShaderCompilingManager->FinishAllCompilation();
#endif
        CaptureName = Name;
        CaptureFrames = 15;
    }

    bool CheckDungeon(UWorld* World, AARPlayerCharacter* Player, AARPortal*& Portal, FVector& ShopApproach)
    {
        int32 EnemyCount = 0;
        AARBoss* Boss = nullptr;
        for (TActorIterator<AAREnemy> It(World); It; ++It)
        {
            if (auto* Found = Cast<AARBoss>(*It)) Boss = Found;
            else ++EnemyCount;
        }
        AARShop* Shop = nullptr;
        for (TActorIterator<AARShop> It(World); It; ++It) Shop = *It;
        for (TActorIterator<AARPortal> It(World); It; ++It) Portal = *It;
        if (!Test.TestTrue(TEXT("Dungeon contains three enemies, one boss, a shop and extraction"), EnemyCount == 3 && Boss && Shop && Portal)) return false;

        const FVector Entry = Player->GetActorLocation();
        auto Reaches = [&](const FString& Label, const FVector& Goal)
        {
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Entry, Goal, Player);
            const bool Connected = Path && Path->IsValid() && !Path->IsPartial();
            if (!Connected)
            {
                auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
                FNavLocation Projected;
                const FVector Extent(110, 110, 350);
                const bool bEntryOnNav = Nav && Nav->ProjectPointToNavigation(Entry, Projected, Extent);
                const bool bGoalOnNav = Nav && Nav->ProjectPointToNavigation(Goal, Projected, Extent);
                Test.AddInfo(FString::Printf(TEXT("%s entry %s nav=%d goal nav=%d partial=%d"), *Label, *Entry.ToCompactString(), bEntryOnNav, bGoalOnNav, Path && Path->IsPartial()));
            }
            return Test.TestTrue(*FString::Printf(TEXT("%s can reach %s (%s)"), *World->GetMapName(), *Label, *Goal.ToCompactString()), Connected);
        };
        bool bConnected = Reaches(TEXT("boss"), Boss->GetActorLocation());
        bConnected &= Reaches(TEXT("extraction"), Portal->GetActorLocation() + FVector(0, 0, 100));
        bool bHasApproach = false;
        for (const FVector& Offset : {FVector(0, -200, 42), FVector(-200, 0, 42), FVector(0, 200, 42), FVector(200, 0, 42)})
        {
            const FVector Goal = Shop->GetActorLocation() + Offset;
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Entry, Goal, Player);
            if (Path && Path->IsValid() && !Path->IsPartial())
            {
                ShopApproach = Goal;
                bHasApproach = true;
                break;
            }
        }
        return Test.TestTrue(TEXT("One side of the shop has a connected interaction point"), bHasApproach) && bConnected;
    }

    bool CheckThirdDungeon(UWorld* World, AARPlayerCharacter* Player, AARPrototypeGameMode* Mode, UARRunGameInstance* State)
    {
        AARExpeditionPassage* ShopDoor = nullptr;
        AARExpeditionPassage* BossDoor = nullptr;
        for (TActorIterator<AARExpeditionPassage> It(World); It; ++It)
        {
            if (It->ActorHasTag(TEXT("ExpeditionShopEntrance"))) ShopDoor = *It;
            if (It->ActorHasTag(TEXT("ExpeditionBossEntrance"))) BossDoor = *It;
        }
        AARShop* Shop = nullptr;
        AARBoss* Boss = nullptr;
        AARPortal* Portal = nullptr;
        int32 Encounters = 0;
        for (TActorIterator<AARShop> It(World); It; ++It) Shop = *It;
        for (TActorIterator<AAREnemy> It(World); It; ++It)
        {
            if (auto* Found = Cast<AARBoss>(*It)) Boss = Found;
            else ++Encounters;
        }
        for (TActorIterator<AARPortal> It(World); It; ++It)
            if (It->ActorHasTag(TEXT("ExpeditionExtraction"))) Portal = *It;
        const bool bRoomsLoaded = World->GetStreamingLevels().Num() == 2 &&
            World->GetStreamingLevels().ContainsByPredicate([](const ULevelStreaming* Level) {
                return Level && Level->GetLoadedLevel() && Level->GetWorldAssetPackageName().EndsWith(TEXT("/Map_Shop"));
            }) &&
            World->GetStreamingLevels().ContainsByPredicate([](const ULevelStreaming* Level) {
                return Level && Level->GetLoadedLevel() && Level->GetWorldAssetPackageName().EndsWith(TEXT("/Map_Boss"));
            });
        if (!Test.TestTrue(TEXT("Dungeon 3 starts with both authored rooms, one merchant, one boss, five enemies and extraction"),
            bRoomsLoaded && Shop && Boss && Portal && Encounters == 5)) return false;
        if (!Test.TestTrue(TEXT("Shop and boss entrances have reciprocal links"),
            ShopDoor && IsValid(ShopDoor->Destination) && ShopDoor->Destination->Destination == ShopDoor &&
            BossDoor && IsValid(BossDoor->Destination) && BossDoor->Destination->Destination == BossDoor)) return false;

        const FVector Stand(0, 0, 96);
        const auto Reaches = [&](const TCHAR* Label, const FVector& From, const FVector& Goal)
        {
            const auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, Goal, Player);
            return Test.TestTrue(Label, Path && Path->IsValid() && !Path->IsPartial());
        };
        const auto Arrive = [&](AARExpeditionPassage* Door)
        {
            Player->SetActorLocation(Door->GetActorLocation() + Stand);
            Player->Interact();
            const FVector Expected = Door->Destination->GetActorTransform().TransformPositionNoScale(Door->Destination->ArrivalOffset);
            return FVector::Dist2D(Expected, Player->GetActorLocation()) < 65;
        };
        const FVector Entry = Player->GetActorLocation();
        const FVector Trader = Shop->GetActorLocation() + FVector(0, 155, 50);
        bool bConnected = Reaches(TEXT("Dungeon 3 entrance reaches its shop gate"), Entry, ShopDoor->GetActorLocation() + Stand);
        bConnected &= Reaches(TEXT("Dungeon 3 entrance reaches its boss gate"), Entry, BossDoor->GetActorLocation() + Stand);
        bConnected &= Reaches(TEXT("Dungeon 3 boss gate can return to extraction"), BossDoor->GetActorLocation() + Stand, Portal->GetActorLocation() + Stand);
        if (!bConnected) return false;

        const int32 OriginalSeed = State->Seed;
        Player->Health->ReceiveDamage(10);
        const float HealthBeforeTravel = Player->Health->CurrentHealth;
        if (!Test.TestTrue(TEXT("Dungeon 3 can accumulate loot before shopping"), State->AddCurrency(40, 1) && Player->Inventory->TryAdd(TEXT("RunicCharm"), 1))) return false;
        if (!Test.TestTrue(TEXT("E enters the dedicated shop from Dungeon 3"), Arrive(ShopDoor))) return false;
        if (!Reaches(TEXT("Shop arrival reaches the counter"), Player->GetActorLocation(), Trader)) return false;
        Player->SetActorLocation(Trader);
        Player->Interact();
        if (!Test.TestTrue(TEXT("Trader sells a run upgrade for 15 gold"), Shop->Purchase(Player, 2) && State->RunGold == 25 && Player->RunPowerBonus == 5)) return false;
        Player->Interact();
        if (!Test.TestTrue(TEXT("E returns from the shop and enters the dedicated boss room"), Arrive(ShopDoor->Destination) && Arrive(BossDoor))) return false;
        if (!Reaches(TEXT("Boss arrival reaches the arena"), Player->GetActorLocation(), Boss->GetActorLocation())) return false;
        if (!Test.TestTrue(TEXT("Room changes preserve the pawn, seed, health, earned loot and the purchased upgrade"),
            UGameplayStatics::GetPlayerCharacter(World, 0) == Player && State->Seed == OriginalSeed &&
            State->RunGold == 25 && State->RunRunes == 1 && Player->Health->CurrentHealth == HealthBeforeTravel &&
            Player->Inventory->Count(TEXT("RunicCharm")) == 1 && Player->RunPowerBonus == 5)) return false;

        Boss->Health->ReceiveDamage(10000);
        if (!Test.TestTrue(TEXT("Defeating the boss is recorded on the same run"), State->bBossDefeated)) return false;
        if (!Test.TestTrue(TEXT("The arena return gate leads back to Dungeon 3"), Arrive(BossDoor->Destination))) return false;
        Mode->ClearRunActors();
        Player->SetActorLocation(Portal->GetActorLocation() + Stand);
        Player->Interact();
        return Test.TestTrue(TEXT("Dungeon 3 starts normal timed extraction"), State->Phase == EARRunPhase::Extracting);
    }
public:
    explicit FARExpeditionCommand(FAutomationTestBase& InTest) : Test(InTest) {}

    bool Update() override
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            Test.AddError(FString::Printf(TEXT("Timed out at expedition stage %d"), Stage));
            return true;
        }
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
        auto* Player = PC ? Cast<AARPlayerCharacter>(PC->GetPawn()) : nullptr;
        auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
        auto* UI = HUD ? HUD->GameUI : nullptr;
        auto* State = World ? World->GetGameInstance<UARRunGameInstance>() : nullptr;
        auto* Mode = World ? World->GetAuthGameMode<AARPrototypeGameMode>() : nullptr;
        if (!Player || !UI || !State || !Mode) return false;
        if (!State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")) || !State->Profile)
        { Test.AddError(TEXT("Use Map_Hub -game -ARTestProfile")); return true; }
        if (CaptureFrames > 0)
        {
            if (--CaptureFrames == 0)
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false);
            return false;
        }
        if (FScreenshotRequest::IsScreenshotRequested()) return false;

        AARPortal* Portal = nullptr;
        FVector ShopApproach;
        switch (Stage)
        {
        case 0:
            if (!Test.TestTrue(TEXT("Starts alive in the town preparation area"), Mode->IsExpeditionTown() && Player->IsAtCamp())) return true;
            PC->SetControlRotation(FRotator(-15, 90, 0));
            Stage = 1;
            Capture(TEXT("V010_Town.png"));
            return false;
        case 1:
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("Both dungeon buttons appear in the town"), UI->FindAction(TEXT("Deploy")) && UI->FindAction(TEXT("DeploySecond")))) return true;
            if (!Test.TestTrue(TEXT("Mage selected in the town"), UI->ExecuteAction(TEXT("Mage")))) return true;
            State->Profile->Stash.Add(TEXT("OakStaff"), 1);
            if (!Test.TestTrue(TEXT("Staff can be prepared"), State->AdjustLoadout(TEXT("OakStaff"), 1, Player))) return true;
            if (!Test.TestTrue(TEXT("First dungeon button starts travel"), UI->ExecuteAction(TEXT("Deploy")))) return true;
            Test.TestEqual(TEXT("Stash is not debited while still in town"), State->Profile->Stash.FindRef(TEXT("OakStaff")), 1);
            Test.TestFalse(TEXT("A second request cannot replace pending travel"), Mode->DepartTown(Player, 1));
            Stage = 2;
            return false;
        case 2:
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon1")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < .85f) return false;
            Test.TestTrue(TEXT("New pawn keeps the Mage and carries one staff"), Player->SelectedClass == EARPlayerClass::Mage && Player->Inventory->Count(TEXT("OakStaff")) == 1 && State->Profile->Stash.FindRef(TEXT("OakStaff")) == 0);
            if (!CheckDungeon(World, Player, Portal, ShopApproach)) return true;
            Stage = 3;
            if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
            {
                Player->SetActorLocation(FVector(-570, -25, 99));
                PC->SetControlRotation(FRotator(-11, 4, 0));
            }
            Capture(TEXT("V010_Dungeon1.png"));
            return false;
        case 3:
            if (!CheckDungeon(World, Player, Portal, ShopApproach)) return true;
            Mode->ClearRunActors();
            Player->SetActorLocation(ShopApproach);
            Player->Interact();
            Test.TestTrue(TEXT("The new shop opens through gameplay interaction"), Player->bShopOpen);
            if (Player->bShopOpen) Player->Interact();
            State->AddCurrency(25, 1);
            Player->SetActorLocation(Portal->GetActorLocation() + FVector(0, 0, 100));
            Player->Interact();
            if (!Test.TestTrue(TEXT("Portal starts the normal timed extraction"), State->Phase == EARRunPhase::Extracting)) return true;
            Stage = 4;
            return false;
        case 4:
            if (State->Phase != EARRunPhase::Result) return false;
            Test.TestTrue(TEXT("Extraction banks gold and returns the carried staff"), State->Summary.bSuccess && State->Profile->BankGold == 25 && State->Profile->Stash.FindRef(TEXT("OakStaff")) == 1);
            Player->ReturnOrRetry();
            Stage = 5;
            return false;
        case 5:
            if (!Mode->IsExpeditionTown()) return false;
            Test.TestTrue(TEXT("Return restores the town spawn and selected class"), Player->IsAtCamp() && Player->SelectedClass == EARPlayerClass::Mage);
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("Second dungeon button starts travel"), UI->ExecuteAction(TEXT("DeploySecond")))) return true;
            Stage = 6;
            return false;
        case 6:
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon2")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < .85f) return false;
            if (!CheckDungeon(World, Player, Portal, ShopApproach)) return true;
            Stage = 7;
            if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
            {
                Player->SetActorLocation(FVector(535, 550, 101));
                PC->SetControlRotation(FRotator(-13, 6, 0));
            }
            Capture(TEXT("V010_Dungeon2.png"));
            return false;
        case 7:
            State->AddCurrency(5, 1);
            Player->Health->ReceiveDamage(1000);
            if (!Test.TestTrue(TEXT("Death settles the second run and preserves the bank"), State->Phase == EARRunPhase::Result && !State->Summary.bSuccess && State->Profile->BankGold == 25)) return true;
            Player->ReturnOrRetry();
            Stage = 8;
            return false;
        case 8:
            if (!Mode->IsExpeditionTown()) return false;
            Test.TestTrue(TEXT("Death returns to town with restored health"), Player->IsAtCamp() && !Player->Health->bDead && Player->Health->CurrentHealth == Player->Health->MaxHealth);
            Player->ToggleCamp(); UI->Refresh();
            if (!Test.TestTrue(TEXT("Third dungeon button starts travel"), UI->ExecuteAction(TEXT("DeployThird")))) return true;
            Stage = 9;
            return false;
        case 9:
            if (!World->GetMapName().EndsWith(TEXT("Map_Dungeon3")) || State->Phase != EARRunPhase::InDungeon || World->GetTimeSeconds() < 1.25f) return false;
            if (!CheckThirdDungeon(World, Player, Mode, State)) return true;
            Stage = 10;
            return false;
        case 10:
            if (State->Phase != EARRunPhase::Result) return false;
            Test.TestTrue(TEXT("Dungeon 3 extraction banks the net gold and equipment"), State->Summary.bSuccess &&
                State->Profile->BankGold == 50 && State->Profile->Stash.FindRef(TEXT("RunicCharm")) == 1 && State->Summary.bBossDefeated);
            Player->ReturnOrRetry();
            Stage = 11;
            return false;
        case 11:
            if (!Mode->IsExpeditionTown()) return false;
            Test.TestTrue(TEXT("Third expedition returns to the normal town start"), Player->IsAtCamp() && State->Phase == EARRunPhase::Hub);
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return true;
    }
};

bool FARExpeditionTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARExpeditionCommand(*this));
    return true;
}
#endif
