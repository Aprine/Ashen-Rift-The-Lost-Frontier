#pragma once

#include "CoreMinimal.h"
#include "ARCoreTypes.h"
#include "GameFramework/Actor.h"
#include "ARBranchWing.generated.h"

class AARExpeditionPassage;
class AAREnemy;
class AARPlayerCharacter;
class AARPortal;
class AARShop;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

enum class EARBranchRoom : uint8 { Entry, Guard, Junction, Supplies, Elite, Exit, Cache, Boss, Count };

// Each edge connects two room indices. The same pair defines a map line and
// the physical doorway. Cells with a shared edge can be crossed both ways.
struct FARBranchPlan
{
    TArray<FIntPoint> Cells;
    TArray<FIntPoint> Edges;
    bool Connects(int32 A, int32 B) const;
    static FARBranchPlan Generate(int32 Seed, bool bIncludeBoss = false);
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
class ASHENRIFT_API AARBranchCache : public AActor
{
    GENERATED_BODY()
public:
    AARBranchCache();
    static constexpr int32 RewardAmount = 2;

    void Configure(AAREnemy* InGuard, bool bGiveRunes);
    bool IsNearby(const AARPlayerCharacter* Player) const;
    bool Claim(AARPlayerCharacter* Player);
    FString Prompt() const;
    bool IsClaimed() const { return bClaimed; }

private:
    TWeakObjectPtr<AAREnemy> Guard;
    bool bRunes = false;
    bool bClaimed = false;
};

UCLASS()
class ASHENRIFT_API AARExpeditionBranchWing : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionBranchWing();
    virtual void PostLoad() override;
    virtual void Tick(float DeltaSeconds) override;

    static constexpr int32 RoomSize = 2000;
    UPROPERTY(EditInstanceOnly, Category="Branch") AARExpeditionPassage* Entrance = nullptr;
    UPROPERTY(EditInstanceOnly, Category="Branch") bool bStandalone = false;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* FloorMesh = nullptr;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* WallMesh = nullptr;
    UPROPERTY(EditAnywhere, Category="Branch") UStaticMesh* FrameMesh = nullptr;
    UPROPERTY(EditAnywhere, EditFixedSize, Category="Branch") TArray<FARBranchRoomStyle> RoomStyles;

    void Prepare(int32 Seed, EARPlayerClass PlayerClass);
    bool Build(AARPlayerCharacter* Player, int32 Tier);
    bool IsPrepared() const { return bPrepared; }
    bool IsBuilt() const { return bBuilt; }
    const FARBranchPlan& Plan() const { return Layout; }
    int32 RoomAt(const FVector& Position) const;
    FVector Center(int32 Room) const;
    FVector Goal(int32 Room) const;
    FVector StartingLocation() const;
    float StartingYaw() const;
    FName EliteReward() const { return Reward; }
    static FName BossReward() { return TEXT("RunicCharm"); }
    bool CacheHoldsRunes() const { return bCacheRunes; }
    bool WasVisited(int32 Room) const { return Visited.Contains(Room); }
    bool WasCacheClaimed() const;
    int32 LivingEnemies(int32 Room) const;
    void RecordVisit(const FVector& Position);
    static bool IsEncounter(int32 Room);
    static const TCHAR* Name(int32 Room);

private:
    UPROPERTY() UMaterialInterface* PortalMaterial = nullptr;
    UPROPERTY() UMaterialInterface* TraderMaterial = nullptr;
    UPROPERTY() AARExpeditionPassage* Return = nullptr;
    UPROPERTY() AARPortal* Exit = nullptr;
    UPROPERTY() AARShop* Shop = nullptr;
    UPROPERTY() AARBranchCache* Cache = nullptr;
    FARBranchPlan Layout;
    TSet<int32> Visited;
    TArray<TArray<TWeakObjectPtr<AAREnemy>>> RoomEnemies;
    FName Reward;
    bool bCacheRunes = false;
    bool bPrepared = false;
    bool bBuilt = false;

    AActor* BuildRoom(int32 Room);
};
