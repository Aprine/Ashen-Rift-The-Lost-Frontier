#pragma once

#include "CoreMinimal.h"
#include "ARCoreTypes.h"
#include "GameFramework/Actor.h"
#include "ARBranchWing.generated.h"

class AARExpeditionPassage;
class AARPlayerCharacter;
class AARPortal;
class AARShop;
class UStaticMesh;
class UMaterialInterface;

enum class EARBranchRoom : uint8 { Entry, Guard, Junction, Supplies, Elite, Exit, Count };

// Each edge connects two room indices. The same pair defines a map line and
// the physical doorway. Cells with a shared edge can be crossed both ways.
struct FARBranchPlan
{
    TArray<FIntPoint> Cells;
    TArray<FIntPoint> Edges;
    bool Connects(int32 A, int32 B) const;
    static FARBranchPlan Generate(int32 Seed);
};

USTRUCT()
struct FARBranchProp
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Room") UStaticMesh* Mesh = nullptr;
    UPROPERTY(EditAnywhere, Category="Room") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="Room") float Yaw = 0;
    UPROPERTY(EditAnywhere, Category="Room") FVector CollisionExtent = FVector::ZeroVector;
};

USTRUCT()
struct FARBranchRoomStyle
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Room") TArray<FARBranchProp> Props;
    UPROPERTY(EditAnywhere, Category="Room") FColor Light = FColor(245, 189, 132);
};

UCLASS()
class ASHENRIFT_API AARExpeditionBranchWing : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionBranchWing();

    static constexpr int32 RoomSize = 2000;
    UPROPERTY(EditInstanceOnly, Category="Branch") AARExpeditionPassage* Entrance = nullptr;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* FloorMesh = nullptr;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* WallMesh = nullptr;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* FrameMesh = nullptr;
    UPROPERTY(EditAnywhere, EditFixedSize, Category="Branch") TArray<FARBranchRoomStyle> RoomStyles;

    void Prepare(int32 Seed, EARPlayerClass PlayerClass);
    bool Build(AARPlayerCharacter* Player);
    bool IsPrepared() const { return bPrepared; }
    bool IsBuilt() const { return bBuilt; }
    const FARBranchPlan& Plan() const { return Layout; }
    int32 RoomAt(const FVector& Position) const;
    FVector Center(int32 Room) const;
    FVector Goal(int32 Room) const;
    FName EliteReward() const { return Reward; }
    static const TCHAR* Name(int32 Room);

private:
    UPROPERTY() UMaterialInterface* PortalMaterial = nullptr;
    UPROPERTY() UMaterialInterface* TraderMaterial = nullptr;
    UPROPERTY() AARExpeditionPassage* Return = nullptr;
    UPROPERTY() AARPortal* Exit = nullptr;
    UPROPERTY() AARShop* Shop = nullptr;
    FARBranchPlan Layout;
    FName Reward;
    bool bPrepared = false;
    bool bBuilt = false;

    AActor* BuildRoom(int32 Room);
};
