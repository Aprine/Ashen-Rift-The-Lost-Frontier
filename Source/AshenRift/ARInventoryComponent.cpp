#include "ARInventoryComponent.h"
UARInventoryComponent::UARInventoryComponent() { PrimaryComponentTick.bCanEverTick = false; }
int32 UARInventoryComponent::Count(FName ID) const
{ const int32* Value = Items.Find(ID); return Value ? *Value : 0; }
bool UARInventoryComponent::CanAdd(FName ID, int32 Quantity) const
{
    if (ID.IsNone() || Quantity <= 0 || Quantity > MaxStack) return false;
    const int32 Existing = Count(ID);
    if (Existing > MaxStack - Quantity || (Existing == 0 && Items.Num() >= Capacity)) return false;
    return true;
}
bool UARInventoryComponent::TryAdd(FName ID, int32 Quantity)
{
    if (!CanAdd(ID, Quantity)) return false;
    const int32 Existing = Count(ID);
    Items.Add(ID, Existing + Quantity);
    OnChanged.Broadcast();
    return true;
}
bool UARInventoryComponent::TryRemove(FName ID, int32 Quantity)
{
    const int32 Existing = Count(ID);
    if (Quantity <= 0 || Existing < Quantity) return false;
    if (Existing == Quantity) Items.Remove(ID); else Items.Add(ID, Existing - Quantity);
    OnChanged.Broadcast();
    return true;
}
void UARInventoryComponent::Clear() { Items.Empty(); OnChanged.Broadcast(); }
