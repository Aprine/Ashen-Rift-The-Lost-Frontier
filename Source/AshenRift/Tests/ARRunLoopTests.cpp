#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../ARPrototype.h"
#include "../ARRunGameInstance.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARRunLoopTest, "AshenRift.Integration.RunLoop",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

class FARRunLoopCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double WaitUntil = 0;
    TWeakObjectPtr<AAREnemy> Target;
public:
    explicit FARRunLoopCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::Game) World = Context.World();
        if (!World) { Test->AddError(TEXT("Run with UE4Editor-Cmd -game on L_FirstPlayable.")); return true; }
        auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
        auto* State = Cast<UARRunGameInstance>(World->GetGameInstance());
        if (!Player || !State) { Test->AddError(TEXT("Playable pawn or run instance missing.")); return true; }
        if (!State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Refusing to test against real profile. Add -ARTestProfile.")); return true; }
        const float Now = World->GetTimeSeconds();
        if (Now < WaitUntil) return false;
        switch (Stage)
        {
        case 0:
        {
            Test->TestTrue(TEXT("Starts in camp"), State->Phase == EARRunPhase::Hub);
            Player->Interact();
            Test->TestTrue(TEXT("E starts dungeon"), State->Phase == EARRunPhase::InDungeon);
            int32 Enemies = 0;
            for (TActorIterator<AAREnemy> It(World); It; ++It) { if (!Target.IsValid()) Target = *It; ++Enemies; }
            Test->TestEqual(TEXT("Dungeon spawns three enemies"), Enemies, 3);
            // Collect actual spawned potion actor through its collection transaction.
            for (TActorIterator<AARLoot> It(World); It; ++It)
                if (It->ItemID == TEXT("Potion")) Test->TestTrue(TEXT("Potion pickup succeeds"), It->TryCollect(Player));
            Player->Health->ReceiveDamage(30);
            Player->UsePotion();
            Test->TestEqual(TEXT("Potion restores health"), Player->Health->CurrentHealth, 100.f);
            Test->TestEqual(TEXT("Potion consumed once"), Player->Inventory->Count(TEXT("Potion")), 1);
            Player->UsePotion();
            Test->TestEqual(TEXT("Full health keeps potion"), Player->Inventory->Count(TEXT("Potion")), 1);
            if (!Target.IsValid()) { Test->AddError(TEXT("No enemy to attack")); return true; }
            Player->SetActorLocation(Target->GetActorLocation() - FVector(150, 0, 0));
            Player->Controller->SetControlRotation(FRotator::ZeroRotator);
            Player->Attack();
            Test->TestEqual(TEXT("Melee hits enemy once"), Target->Health->CurrentHealth, 35.f);
            Player->Attack();
            Test->TestEqual(TEXT("Attack cooldown blocks repeat click"), Target->Health->CurrentHealth, 35.f);
            WaitUntil = Now + .65;
            break;
        }
        case 1:
            Test->TestTrue(TEXT("Enemy pursuit and attack damage player"), Player->Health->CurrentHealth < 100.f);
            Player->Attack();
            Test->TestTrue(TEXT("Enemy survives second hit"), Target.IsValid());
            WaitUntil = Now + .65;
            break;
        case 2:
        {
            Player->Attack();
            Test->TestFalse(TEXT("Third hit kills enemy"), Target.IsValid() && !Target->IsActorBeingDestroyed());
            int32 GoldDrops = 0;
            for (TActorIterator<AARLoot> It(World); It; ++It)
                if (It->ItemID == TEXT("Gold"))
                {
                    ++GoldDrops;
                    Test->TestTrue(TEXT("Gold pickup succeeds"), It->TryCollect(Player));
                    Test->TestFalse(TEXT("Destroyed loot cannot pay twice"), It->TryCollect(Player));
                }
            Test->TestEqual(TEXT("Enemy drops once"), GoldDrops, 1);
            Test->TestEqual(TEXT("Pickup credits run wallet"), State->RunGold, 10);
            // Remove other enemies so extraction timing is deterministic.
            for (TActorIterator<AAREnemy> It(World); It; ++It) It->Destroy();
            Player->SetActorLocation(FVector(4800, -650, 100));
            Player->Interact();
            Test->TestTrue(TEXT("Portal starts extraction"), State->Phase == EARRunPhase::Extracting);
            Player->SetActorLocation(FVector(4800, 0, 100));
            WaitUntil = Now + .15;
            break;
        }
        case 3:
            Test->TestTrue(TEXT("Leaving portal cancels"), State->Phase == EARRunPhase::InDungeon);
            Player->SetActorLocation(FVector(4800, -650, 100));
            Player->Interact();
            Player->Health->ReceiveDamage(5);
            Test->TestTrue(TEXT("Damage cancels extraction"), State->Phase == EARRunPhase::InDungeon);
            Player->Interact();
            WaitUntil = Now + 3.2;
            break;
        case 4:
        {
            Test->TestTrue(TEXT("Extraction reaches result"), State->Phase == EARRunPhase::Result && State->Summary.bSuccess);
            Test->TestEqual(TEXT("Extracted gold is banked"), State->Profile->BankGold, 10);
            Test->TestEqual(TEXT("Extracted item is stashed"), State->Profile->Stash.FindRef(TEXT("Potion")), 1);
            Test->TestEqual(TEXT("Run inventory cleared"), Player->Inventory->Items.Num(), 0);
            Test->TestFalse(TEXT("Settlement cannot run twice"), State->SettleRun(true, Player->Inventory));
            auto* Saved = Cast<UARProfile>(UGameplayStatics::LoadGameFromSlot(State->GetProfileSlot(), 0));
            Test->TestNotNull(TEXT("Extracted profile loads from disk"), Saved);
            if (Saved) Test->TestEqual(TEXT("Disk bank equals committed bank"), Saved->BankGold, 10);
            Player->ReturnOrRetry();
            Test->TestTrue(TEXT("Return reaches camp"), State->Phase == EARRunPhase::Hub);
            Player->Interact();
            State->AddCurrency(20, 0);
            Player->Health->ReceiveDamage(1000);
            Test->TestTrue(TEXT("Death reaches failed result"), State->Phase == EARRunPhase::Result && !State->Summary.bSuccess);
            Test->TestEqual(TEXT("Death loses all run gold"), State->Summary.LostGold, 20);
            Test->TestEqual(TEXT("Death preserves permanent bank"), State->Profile->BankGold, 10);
            Player->ReturnOrRetry();
            Test->TestFalse(TEXT("Respawn revives player"), Player->Health->bDead);
            Player->Interact();
            Test->TestTrue(TEXT("Another run can start"), State->Phase == EARRunPhase::InDungeon);
            Test->TestEqual(TEXT("Next run starts with zero gold"), State->RunGold, 0);
            Player->Health->ReceiveDamage(1000);
            Player->ReturnOrRetry();
            UGameplayStatics::DeleteGameInSlot(State->GetProfileSlot(), 0);
            return true;
        }
        }
        ++Stage;
        return false;
    }
};
bool FARRunLoopTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")))
    { AddError(TEXT("Use -ARTestProfile to isolate saves.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARRunLoopCommand(this));
    return true;
}
#endif
