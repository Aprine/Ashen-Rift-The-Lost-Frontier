#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARDungeon.generated.h"

class UStaticMesh;
class AARPlayerCharacter;
class AAREnemy;
class AARBoss;

// Shared footprints and doorways are authored in the map; these room prefabs
// provide deterministic interior geometry at runtime.
UCLASS()
class ASHENRIFT_API AARDungeonRoom : public AActor
{
    GENERATED_BODY()
public:
    AARDungeonRoom();
    void BuildInterior(int32 Kind);
    UPROPERTY(BlueprintReadOnly) int32 RoomKind = 0;
private:
    UPROPERTY() UStaticMesh* Cube;
    void Block(const FVector& Position, const FVector& Size);
};

UCLASS()
class ASHENRIFT_API AARDungeonDirector : public AActor
{
    GENERATED_BODY()
public:
    AARDungeonDirector();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseFixedSeed = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FixedSeed = 1337;
    UPROPERTY(BlueprintReadOnly) TArray<int32> RoomOrder;
    UPROPERTY(BlueprintReadOnly) int32 CurrentSeed = 0;
    UFUNCTION(BlueprintPure, Category="Dungeon") static TArray<int32> MakeRoomOrder(int32 Seed);
    UFUNCTION(BlueprintPure, Category="Dungeon") static FString RoomName(int32 Kind);
    UFUNCTION(BlueprintCallable, Category="Dungeon") void Generate(int32 Seed);
    void Populate(AARPlayerCharacter* Player, TSubclassOf<AAREnemy> EnemyClass, TSubclassOf<AAREnemy> RangedClass, TSubclassOf<AAREnemy> EliteClass, TSubclassOf<AARBoss> BossClass);
    FVector Entry() const { return FVector(1450, 0, 100); }
    UPROPERTY() TArray<AARDungeonRoom*> Rooms;
};

// Editor-only implementation creates a real, saved brush for NavMesh bounds.
UCLASS()
class ASHENRIFT_API UARDungeonEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static AActor* CreateNavigationBounds(UObject* WorldContext, FVector Center, FVector Extent);
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static bool ResizeNavigationBounds(AActor* Actor, FVector Center, FVector Extent);
};
