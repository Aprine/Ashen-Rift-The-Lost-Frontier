#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../ARDungeon.h"
#include "../ARPrototype.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../AREquipmentComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARProfile.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARSeedTest, "AshenRift.Core.RoomSeeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FARSeedTest::RunTest(const FString& Parameters)
{
    const auto First = AARDungeonDirector::MakeRoomOrder(1337);
    TestTrue(TEXT("Same seed reproduces full room order"), First == AARDungeonDirector::MakeRoomOrder(1337));
    TSet<FString> Seen;
    for (int32 Seed = 0; Seed < 100; ++Seed)
    {
        const auto Order = AARDungeonDirector::MakeRoomOrder(Seed);
        TestEqual(TEXT("Always three combat rooms"), Order.Num(), 3);
        TSet<int32> Types;
        for (int32 Kind : Order) { Types.Add(Kind); TestTrue(TEXT("Only valid room prefabs"), Kind >= 0 && Kind <= 2); }
        TestEqual(TEXT("Room selection has no duplicates"), Types.Num(), 3);
        Seen.Add(FString::Printf(TEXT("%d%d%d"), Order[0], Order[1], Order[2]));
    }
    TestEqual(TEXT("Seed range exercises every room permutation"), Seen.Num(), 6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARDungeonTest, "AshenRift.Integration.RandomDungeon",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARDungeonCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    float WaitUntil = 0;
    float Deadline = 0;
    FVector EnemyStart;
    TWeakObjectPtr<AAREnemy> Enemy;
    TWeakObjectPtr<AARDungeonDirector> Director;
    TArray<int32> ExpectedOrder;
    FString CaptureName;
    int32 CaptureFrames = 0;
    bool bCaptureRequested = false;
public:
    explicit FARDungeonCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    void Capture(const FString& Name)
    { if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture"))) { CaptureName = Name; CaptureFrames = 0; bCaptureRequested = false; } }
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        if (!World) { Test->AddError(TEXT("Run L_RandomDungeon with -game.")); return true; }
        auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
        auto* State = Cast<UARRunGameInstance>(World->GetGameInstance());
        if (!Player || !State || !State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Missing player or isolated test profile.")); return true; }
        if (!CaptureName.IsEmpty())
        {
            if (++CaptureFrames < 3) return false;
            if (!bCaptureRequested) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false); bCaptureRequested = true; return false; }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            CaptureName.Empty();
            return false;
        }
        const float Now = World->GetTimeSeconds();
        if (Now < WaitUntil) return false;
        switch (Stage)
        {
        case 0:
        {
            for (TActorIterator<AARDungeonDirector> It(World); It; ++It) Director = *It;
            if (!Director.IsValid()) { Test->AddError(TEXT("Random director missing.")); return true; }
            // Pick a deterministic layout whose first room is the central barrier.
            int32 Seed = 0;
            while (AARDungeonDirector::MakeRoomOrder(Seed)[0] != 1) ++Seed;
            Director->bUseFixedSeed = true;
            Director->FixedSeed = Seed;
            Player->Interact();
            ExpectedOrder = AARDungeonDirector::MakeRoomOrder(Seed);
            Test->TestTrue(TEXT("Gameplay uses selected seed"), Director->RoomOrder == ExpectedOrder && State->Seed == Seed);
            Test->TestEqual(TEXT("Three room prefabs generated"), Director->Rooms.Num(), 3);
            int32 Enemies = 0, Bosses = 0;
            for (TActorIterator<AAREnemy> It(World); It; ++It) { if (Cast<AARBoss>(*It)) ++Bosses; else ++Enemies; }
            Test->TestEqual(TEXT("Two enemies in each combat room"), Enemies, 6);
            Test->TestEqual(TEXT("One boss at final room"), Bosses, 1);
            for (TActorIterator<AARLoot> It(World); It; ++It) if (It->ItemID == TEXT("IronSword")) It->TryCollect(Player);
            Test->TestTrue(TEXT("Matching weapon can equip"), Player->Equipment->ToggleEquip(TEXT("IronSword")));
            Test->TestEqual(TEXT("Weapon supplies real damage bonus"), Player->TotalDamageBonus(), 8);
            Test->TestEqual(TEXT("Equipping does not duplicate or consume item"), Player->Inventory->Count(TEXT("IronSword")), 1);
            Test->TestFalse(TEXT("Unowned gear rejected"), Player->Equipment->ToggleEquip(TEXT("LeatherArmor")));
            Player->Inventory->TryAdd(TEXT("OakStaff"), 1);
            Test->TestFalse(TEXT("Wrong class weapon rejected"), Player->Equipment->ToggleEquip(TEXT("OakStaff")));
            Player->Inventory->TryAdd(TEXT("LeatherArmor"), 1);
            Player->Inventory->TryAdd(TEXT("RunicCharm"), 1);
            Player->Health->ReceiveDamage(30);
            Test->TestTrue(TEXT("Armor equip succeeds"), Player->Equipment->ToggleEquip(TEXT("LeatherArmor")));
            Test->TestEqual(TEXT("Armor increases max HP"), Player->Health->MaxHealth, 120.f);
            Test->TestEqual(TEXT("Armor cannot heal on equip"), Player->Health->CurrentHealth, 70.f);
            Player->Equipment->ToggleEquip(TEXT("LeatherArmor"));
            Player->Equipment->ToggleEquip(TEXT("LeatherArmor"));
            Test->TestEqual(TEXT("Repeated equip cannot generate healing"), Player->Health->CurrentHealth, 70.f);
            Player->Equipment->ToggleEquip(TEXT("RunicCharm"));
            Test->TestEqual(TEXT("Equipment bonuses combine"), Player->TotalDamageBonus(), 12);
            Player->Inventory->TryRemove(TEXT("IronSword"), 1);
            Test->TestTrue(TEXT("Removing owned item clears equipment slot"), Player->Equipment->Weapon.IsNone());
            Player->Inventory->TryAdd(TEXT("IronSword"), 1);
            Player->Equipment->ToggleEquip(TEXT("IronSword"));
            auto* Dummy = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), Player->GetActorLocation() + FVector(150, 0, 0), FRotator::ZeroRotator);
            if (Dummy)
            {
                Player->Attack();
                Test->TestEqual(TEXT("Equipped gear increases actual attack damage"), Dummy->Health->CurrentHealth, 23.f);
            }
            else Test->AddError(TEXT("Damage test enemy failed to spawn."));
            // Clear unrelated enemies, then explicitly test pursuit around a barrier.
            for (TActorIterator<AAREnemy> It(World); It; ++It) It->Destroy();
            EnemyStart = FVector(1850, 0, 100);
            Enemy = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), EnemyStart, FRotator::ZeroRotator);
            Player->SetActorLocation(FVector(2950, 0, 100));
            if (!Enemy.IsValid()) { Test->AddError(TEXT("Navigation test enemy failed to spawn.")); return true; }
            Enemy->DetectRange = 2000;
            // Start visible so it acquires the player before the blocked route.
            Player->SetActorLocation(FVector(1850, 600, 100));
            Deadline = Now + 15;
            WaitUntil = Now + .2f;
            break;
        }
        case 1:
            Player->SetActorLocation(FVector(2950, 0, 100));
            Stage = 2;
            WaitUntil = Now + .8f;
            return false;
        case 2:
        {
            auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(1850, 0, 100), FVector(2950, 0, 100), Player);
            if ((!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 3) && Now < Deadline) return false;
            if (!Path || !Path->IsValid() || Path->IsPartial()) { Test->AddError(TEXT("Runtime NavMesh did not create a full path around barrier.")); return true; }
            Test->TestTrue(TEXT("NavMesh path bends around obstacle"), Path->PathPoints.Num() >= 3);
            auto* EscapePath = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(1450, 0, 100), FVector(7200, -650, 100), Player);
            auto* BossPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(1450, 0, 100), FVector(12000, 0, 150), Player);
            Test->TestTrue(TEXT("Dungeon entry connects to extraction"), EscapePath && EscapePath->IsValid() && !EscapePath->IsPartial());
            Test->TestTrue(TEXT("Dungeon entry connects to boss room"), BossPath && BossPath->IsValid() && !BossPath->IsPartial());
            Test->TestTrue(TEXT("Enemy uses a multi-point path"), Enemy->NavigationPointCount >= 3);
            Test->TestTrue(TEXT("Enemy moves despite blocked direct sight"), FVector::Dist2D(Enemy->GetActorLocation(), EnemyStart) > 20);
            Capture(TEXT("M3_Navigation.png"));
            Deadline = Now + 14;
            Stage = 3;
            WaitUntil = Now + 1;
            return false;
        }
        case 3:
        {
            if (Enemy.IsValid() && FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) > 190 && Now < Deadline) return false;
            Test->TestTrue(TEXT("Enemy reaches player by going around barrier"), Enemy.IsValid() && FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) <= 190);
            if (Enemy.IsValid()) Enemy->Destroy();
            Player->bInventoryVisible = true;
            Capture(TEXT("M3_Equipment.png"));
            Stage = 30;
            WaitUntil = Now + .2f;
            return false;
        }
        case 30:
            State->AddCurrency(25, 0);
            Player->SetActorLocation(FVector(7200, -650, 100));
            Player->Interact();
            Stage = 4;
            WaitUntil = Now + 3.2f;
            return false;
        case 4:
        {
            Test->TestTrue(TEXT("New portal completes extraction"), State->Phase == EARRunPhase::Result && State->Summary.bSuccess);
            Test->TestEqual(TEXT("Equipped weapon is banked once as owned loot"), State->Profile->Stash.FindRef(TEXT("IronSword")), 1);
            Test->TestEqual(TEXT("Armor is banked"), State->Profile->Stash.FindRef(TEXT("LeatherArmor")), 1);
            Test->TestTrue(TEXT("Settlement clears equipment"), Player->Equipment->Weapon.IsNone() && Player->Equipment->Armor.IsNone() && Player->Equipment->Charm.IsNone());
            auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestTrue(TEXT("Equipment ownership persists on disk"), Saved && Saved->Stash.FindRef(TEXT("IronSword")) == 1);
            Player->ReturnOrRetry();
            Player->Interact();
            Test->TestTrue(TEXT("Fixed seed reproduces next run layout"), Director->RoomOrder == ExpectedOrder);
            Player->Inventory->TryAdd(TEXT("LeatherArmor"), 1);
            Player->Equipment->ToggleEquip(TEXT("LeatherArmor"));
            Player->Health->ReceiveDamage(1000);
            Test->TestTrue(TEXT("Death drops equipped run gear"), Player->Equipment->Armor.IsNone() && Player->Inventory->Items.Num() == 0);
            Test->TestEqual(TEXT("Death preserves previously extracted armor"), State->Profile->Stash.FindRef(TEXT("LeatherArmor")), 1);
            Player->ReturnOrRetry();
            Test->TestEqual(TEXT("Respawn removes temporary armor maximum"), Player->Health->MaxHealth, 100.f);
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        }
        ++Stage;
        return false;
    }
};
bool FARDungeonTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARDungeonCommand(this));
    return true;
}
#endif
