#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARHealthTest, "AshenRift.Core.Health",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FARHealthTest::RunTest(const FString& Parameters)
{
    UARHealthComponent* Health = NewObject<UARHealthComponent>();
    Health->MaxHealth = 100.f;
    Health->ResetHealth();
    Health->ReceiveDamage(-20.f);
    TestEqual(TEXT("Negative damage cannot heal or damage"), Health->CurrentHealth, 100.f);
    TestEqual(TEXT("Full health cannot consume a potion"), Health->Heal(30.f), 0.f);
    Health->ReceiveDamage(20.f);
    TestEqual(TEXT("Healing reports actual restored health"), Health->Heal(30.f), 20.f);
    Health->ReceiveDamage(500.f);
    TestTrue(TEXT("Lethal damage sets death"), Health->bDead);
    TestEqual(TEXT("Lethal damage clamps to zero"), Health->CurrentHealth, 0.f);
    TestEqual(TEXT("Dead actor cannot heal"), Health->Heal(100.f), 0.f);
    Health->ReceiveDamage(10.f);
    TestEqual(TEXT("Further damage leaves death stable"), Health->CurrentHealth, 0.f);
    Health->ResetHealth();
    TestFalse(TEXT("Respawn clears death"), Health->bDead);
    TestEqual(TEXT("Respawn restores full health"), Health->CurrentHealth, 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARInventoryTest, "AshenRift.Core.Inventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FARInventoryTest::RunTest(const FString& Parameters)
{
    UARInventoryComponent* Inventory = NewObject<UARInventoryComponent>();
    Inventory->Capacity = 1;
    Inventory->MaxStack = 5;
    TestFalse(TEXT("Reject empty item IDs"), Inventory->TryAdd(NAME_None, 1));
    TestFalse(TEXT("Reject negative quantities"), Inventory->TryAdd(TEXT("Potion"), -1));
    TestTrue(TEXT("Add first stack"), Inventory->TryAdd(TEXT("Potion"), 3));
    TestFalse(TEXT("Full inventory rejects a new item"), Inventory->TryAdd(TEXT("Sword"), 1));
    TestTrue(TEXT("Full inventory accepts existing stack"), Inventory->TryAdd(TEXT("Potion"), 2));
    TestFalse(TEXT("Oversized pickup is rejected atomically"), Inventory->TryAdd(TEXT("Potion"), 1));
    TestEqual(TEXT("Rejected pickup leaves stack intact"), Inventory->Count(TEXT("Potion")), 5);
    TestFalse(TEXT("Cannot remove more than owned"), Inventory->TryRemove(TEXT("Potion"), 6));
    TestTrue(TEXT("Remove entire stack"), Inventory->TryRemove(TEXT("Potion"), 5));
    TestTrue(TEXT("Removing stack releases its slot"), Inventory->TryAdd(TEXT("Sword"), 1));
    Inventory->Clear();
    TestEqual(TEXT("Settlement clears inventory"), Inventory->Items.Num(), 0);
    return true;
}
#endif
