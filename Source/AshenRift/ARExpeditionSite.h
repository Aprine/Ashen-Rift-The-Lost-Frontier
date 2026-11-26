#pragma once

#include "CoreMinimal.h"
#include "ARCoreTypes.h"
#include "GameFramework/Actor.h"
#include "ARExpeditionSite.generated.h"

class AARPlayerCharacter;
class AARExpeditionBranchWing;
class AAREnemy;
class AARPortal;
class UStaticMeshComponent;

enum class EARExpeditionEnemy : uint8 { Melee, Ranged, Elite };

namespace ARExpedition
{
    int32 GoldPercent(int32 Tier);
    int32 EquipmentChance(int32 Tier);
    int32 ChooseDungeon(int32 Seed, int32 MapCount);
    TArray<EARExpeditionEnemy> MakeEncounters(int32 Count, int32 Seed, int32 Tier);
    FName ChooseEquipment(FRandomStream& Random, EARPlayerClass PlayerClass);
}

// Place one marker per authored level. Its transform defines the preparation
// point in town or the entry point in a dungeon. Spawn locations are local to it.
UCLASS()
class ASHENRIFT_API AARExpeditionSite : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionSite();

    UPROPERTY(EditAnywhere, Category="Expedition") bool bTown = false;
    UPROPERTY(EditInstanceOnly, Category="Expedition") AARExpeditionBranchWing* StandaloneWing = nullptr;
    UPROPERTY(EditAnywhere, Category="Expedition", meta=(MakeEditWidget)) TArray<FVector> EncounterOffsets;
    UPROPERTY(EditAnywhere, Category="Expedition", meta=(MakeEditWidget)) FVector BossOffset;
    UPROPERTY(EditAnywhere, Category="Expedition", meta=(MakeEditWidget)) FVector SupplyOffset;
};

UCLASS()
class ASHENRIFT_API AARExpeditionPassage : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionPassage();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Passage") UStaticMeshComponent* Mesh;
    UPROPERTY(EditInstanceOnly, Category="Passage") AARExpeditionPassage* Destination = nullptr;
    UPROPERTY(EditInstanceOnly, Category="Passage") AARExpeditionBranchWing* BranchWing = nullptr;
    UPROPERTY(EditInstanceOnly, Category="Passage") FText Prompt;
    UPROPERTY(EditAnywhere, Category="Passage", meta=(MakeEditWidget)) FVector ArrivalOffset = FVector(210, 0, 102);

    bool CanUse(const AARPlayerCharacter* Player) const;
    bool Use(AARPlayerCharacter* Player);
};

UENUM()
enum class EARExpeditionRoute : uint8 { None, Supplies, Elite, Extract };

UCLASS()
class ASHENRIFT_API AARExpeditionRouteBoard : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionRouteBoard();

    UPROPERTY(EditInstanceOnly, Category="Routes") AARExpeditionPassage* ShopPassage = nullptr;
    UPROPERTY(EditInstanceOnly, Category="Routes") AARPortal* EntrancePortal = nullptr;
    UPROPERTY(EditInstanceOnly, Category="Routes") AARExpeditionBranchWing* BranchWing = nullptr;
    UPROPERTY(EditAnywhere, Category="Routes", meta=(MakeEditWidget)) TArray<FVector> EliteOffsets;
    UPROPERTY(EditAnywhere, Category="Routes", meta=(MakeEditWidget)) TArray<FVector> ExtractionOffsets;

    void InitializeRun(int32 Seed, EARPlayerClass PlayerClass);
    bool CanOpen(const AARPlayerCharacter* Player) const;
    bool Choose(EARExpeditionRoute Route, AARPlayerCharacter* Player);
    FVector Target(EARExpeditionRoute Route) const;
    FName EliteReward() const { return RewardItem; }
    bool HasStartedElite() const { return bEliteStarted; }
    bool HasClearedElite() const { return bEliteStarted && !Elite.IsValid(); }

private:
    UPROPERTY() AARPortal* RoutePortal = nullptr;
    TWeakObjectPtr<AAREnemy> Elite;
    FVector ElitePosition = FVector::ZeroVector;
    FName RewardItem;
    bool bReady = false;
    bool bEliteStarted = false;
};
