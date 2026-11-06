#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "../ARDungeon.h"
#include "../ARPrototype.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARTeamMapTest, "AshenRift.Integration.TeamMap",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARTeamMapCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0, OrderIndex = 0;
    float WaitUntil = 0, Deadline = 0;
    TArray<int32> Seeds;
    TWeakObjectPtr<AARDungeonDirector> Director;
    TWeakObjectPtr<AActor> Camp;
    FString CaptureName;
    int32 Frames = 0;
    bool bRequested = false;

    void Capture(const TCHAR* Name)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) return;
#if WITH_EDITOR
        GShaderCompilingManager->FinishAllCompilation();
#endif
        CaptureName = Name; Frames = 0; bRequested = false;
    }
    void CheckCamp(UWorld* World)
    {
        Test->TestTrue(TEXT("Authored camp survives every run and is reused"), Camp.IsValid() && Director->EnsureCampLayout() == Camp.Get());
        int32 Count = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (IsValid(*It) && It->GetOwner() == Director.Get() && It->IsA(Director->CampLayout)) ++Count;
        Test->TestEqual(TEXT("Exactly one camp prefab is alive"), Count, 1);
    }
public:
    explicit FARTeamMapCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* Player = World ? Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
        auto* State = World ? Cast<UARRunGameInstance>(World->GetGameInstance()) : nullptr;
        auto* Mode = World ? Cast<AARPrototypeGameMode>(World->GetAuthGameMode()) : nullptr;
        if (!Player || !State || !Mode || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")) || !World->GetMapName().Contains(TEXT("L_FrontierDungeon")))
        { Test->AddError(TEXT("Use L_FrontierDungeon -game -ARTestProfile.")); return true; }
        if (!CaptureName.IsEmpty())
        {
            if (++Frames < 4) return false;
            if (!bRequested)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false);
                bRequested = true; return false;
            }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            CaptureName.Empty(); return false;
        }
        const float Now = World->GetTimeSeconds();
        if (Now < WaitUntil) return false;
        switch (Stage)
        {
        case 0:
        {
            for (TActorIterator<AARDungeonDirector> It(World); It; ++It) Director = *It;
            if (!Director.IsValid() || !Director->CampLayout || Director->RoomLayouts.Num() != 3)
            { Test->AddError(TEXT("Contributed map is missing its camp/room assignments.")); return true; }
            Camp = Director->EnsureCampLayout();
            if (!Camp.IsValid()) { Test->AddError(TEXT("Camp prefab failed to spawn.")); return true; }
            CheckCamp(World);
            TArray<UStaticMeshComponent*> CampMeshes;
            Camp->GetComponents(CampMeshes);
            Test->TestTrue(TEXT("Hand-authored camp furnishings were retained"), CampMeshes.Num() >= 12);
            for (auto* Mesh : CampMeshes) Test->TestTrue(TEXT("Camp mesh dependency resolves"), Mesh->GetStaticMesh() != nullptr);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ARCampSpawn), false, Player);
            Test->TestFalse(TEXT("Camp respawn capsule is clear of furniture"), World->OverlapBlockingTestByChannel(
                FVector(-700, 0, 120), FQuat::Identity, ECC_Pawn, Player->GetCapsuleComponent()->GetCollisionShape(), Query));
            for (int32 Door = 0; Door < 5; ++Door)
            {
                const float X = 1200 + Door * 2400;
                FHitResult Hit;
                const bool bBlocked = World->SweepSingleByChannel(Hit, FVector(X - 160, 0, 120), FVector(X + 160, 0, 120),
                    FQuat::Identity, ECC_Pawn, Player->GetCapsuleComponent()->GetCollisionShape(), Query);
                Test->TestFalse(*FString::Printf(TEXT("Door %d admits the full player capsule (%s)"), Door, *GetNameSafe(Hit.GetActor())), bBlocked);
                if (bBlocked) Test->AddInfo(FString::Printf(TEXT("Door hit at %s normal %s component %s"), *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString(), *GetNameSafe(Hit.Component.Get())));
                for (float Z : {100.f, 200.f, 300.f})
                {
                    FHitResult Line;
                    const bool bLine = World->LineTraceSingleByChannel(Line, FVector(X - 180, 0, Z), FVector(X + 180, 0, Z), ECC_Visibility, Query);
                    Test->AddInfo(FString::Printf(TEXT("Door %d center line Z=%.0f blocked=%d actor=%s point=%s"), Door, Z, bLine, *GetNameSafe(Line.GetActor()), *Line.ImpactPoint.ToString()));
                }
            }
            TSet<FString> Seen;
            for (int32 Seed = 0; Seed < 100 && Seeds.Num() < 6; ++Seed)
            {
                const auto Order = AARDungeonDirector::MakeRoomOrder(Seed);
                const FString Key = FString::Printf(TEXT("%d%d%d"), Order[0], Order[1], Order[2]);
                if (!Seen.Contains(Key)) { Seen.Add(Key); Seeds.Add(Seed); }
            }
            Test->TestEqual(TEXT("Six distinct layout orders are exercised"), Seeds.Num(), 6);
            Player->SetActorLocation(FVector(-250, -430, 120));
            if (Player->Controller) Player->Controller->SetControlRotation(FRotator(-18, 145, 0));
            Capture(TEXT("M8_CampLayout.png"));
            Stage = 1; return false;
        }
        case 1:
            Player->SetActorLocation(FVector(-700, 0, 120));
            Director->bUseFixedSeed = true;
            Director->FixedSeed = Seeds[0];
            Player->Interact();
            Test->TestTrue(TEXT("Existing camp action enters the contributed dungeon"), State->Phase == EARRunPhase::InDungeon);
            Mode->ClearRunActors();
            Stage = 2; return false;
        case 2:
        {
            Director->Generate(Seeds[OrderIndex]);
            Test->TestEqual(TEXT("Every order creates all three room prefabs"), Director->Rooms.Num(), 3);
            const auto Order = AARDungeonDirector::MakeRoomOrder(Seeds[OrderIndex]);
            Test->TestTrue(TEXT("Seed reproduces contributed Blueprint order"), Director->RoomOrder == Order);
            const int32 Slots[] = {1, 2, 4};
            for (int32 I = 0; I < Director->Rooms.Num(); ++I)
            {
                auto* Room = Director->Rooms[I];
                Test->TestTrue(TEXT("Runtime uses the authored Blueprint class"), Room && Room->IsA(Director->RoomLayouts[Order[I]]));
                Test->TestTrue(TEXT("Room is anchored at the original connected footprint"), Room && Room->GetActorLocation().Equals(FVector(Slots[I] * 2400, 0, 0), .1f));
            }
            Deadline = Now + 15; WaitUntil = Now + .8f;
            Stage = 3; return false;
        }
        case 3:
        {
            bool bConnected = true;
            TArray<FString> Paths;
            // Query a walkable interaction position beside the solid trader,
            // rather than asking navigation to end inside its mesh.
            for (const FVector& Goal : {FVector(-700, 0, 120), FVector(7200, 430, 120), FVector(7200, -650, 120), FVector(12000, 0, 150)})
            {
                auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(1450, 0, 120), Goal, Player);
                bConnected &= Path && Path->IsValid() && !Path->IsPartial();
                Paths.Add(FString::Printf(TEXT("Goal %s valid=%d partial=%d points=%d end=%s"), *Goal.ToString(), Path && Path->IsValid(), Path && Path->IsPartial(), Path ? Path->PathPoints.Num() : 0,
                    Path && Path->PathPoints.Num() ? *Path->PathPoints.Last().ToString() : TEXT("none")));
            }
            if (!bConnected && Now < Deadline) return false;
            Test->TestTrue(*FString::Printf(TEXT("Order %d connects camp, trader, extraction and boss on NavMesh"), OrderIndex), bConnected);
            if (!bConnected)
            {
                for (const auto& Path : Paths) Test->AddInfo(Path);
                for (int32 Door = 0; Door < 5; ++Door)
                {
                    const float X = 1200 + Door * 2400;
                    auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(X - 250, 0, 120), FVector(X + 250, 0, 120), Player);
                    Test->AddInfo(FString::Printf(TEXT("Door %d local nav valid=%d partial=%d"), Door, Path && Path->IsValid(), Path && Path->IsPartial()));
                }
                Player->SetActorLocation(FVector(750, 0, 120));
                if (Player->Controller) Player->Controller->SetControlRotation(FRotator(-5, 0, 0));
                Capture(TEXT("M8_DoorDiagnostic.png"));
                Stage = 90; return false;
            }
            CheckCamp(World);
            if (++OrderIndex < Seeds.Num()) { Stage = 2; return false; }
            Player->SetActorLocation(FVector(1800, -680, 120));
            if (Player->Controller) Player->Controller->SetControlRotation(FRotator(-12, 25, 0));
            Capture(TEXT("M8_DungeonLayout.png"));
            Stage = 4; return false;
        }
        case 4:
            Player->Inventory->TryAdd(TEXT("IronSword"), 1);
            State->AddCurrency(25, 1);
            Player->SetActorLocation(FVector(7200, -650, 120));
            Player->Interact();
            Test->TestTrue(TEXT("Contributed level retains the existing extraction portal"), State->Phase == EARRunPhase::Extracting);
            Stage = 5; WaitUntil = Now + 3.3f; return false;
        case 5:
            Test->TestTrue(TEXT("Contributed map extraction commits loot and currency"), State->Phase == EARRunPhase::Result && State->Summary.bSuccess && State->Profile->BankGold == 25 && State->Profile->BankRunes == 1 && State->Profile->Stash.FindRef(TEXT("IronSword")) == 1);
            Player->ReturnOrRetry();
            CheckCamp(World);
            Test->TestTrue(TEXT("Extraction returns to a usable camp spawn"), State->Phase == EARRunPhase::Hub && Player->IsAtCamp());
            Player->Interact();
            Test->TestTrue(TEXT("A second run can start in the contributed level"), State->Phase == EARRunPhase::InDungeon);
            Player->Inventory->TryAdd(TEXT("LeatherArmor"), 1);
            State->AddCurrency(10, 1);
            Player->Health->ReceiveDamage(1000);
            Test->TestTrue(TEXT("Death discards new run loot without touching extracted stash"), State->Phase == EARRunPhase::Result && !State->Summary.bSuccess && Player->Inventory->Items.Num() == 0 && State->Profile->BankGold == 25 && State->Profile->Stash.FindRef(TEXT("IronSword")) == 1);
            Player->ReturnOrRetry();
            CheckCamp(World);
            Test->TestTrue(TEXT("Death returns to camp with full health"), State->Phase == EARRunPhase::Hub && Player->IsAtCamp() && Player->Health->CurrentHealth == Player->Health->MaxHealth);
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        case 90:
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        return true;
    }
};

bool FARTeamMapTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARTeamMapCommand(this));
    return true;
}
#endif
