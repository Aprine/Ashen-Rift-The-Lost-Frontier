#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARExpeditionSite.generated.h"

class AARPlayerCharacter;
class UStaticMeshComponent;

// Place one marker per authored level. Its transform defines the preparation
// point in town or the entry point in a dungeon. Spawn locations are local to it.
UCLASS()
class ASHENRIFT_API AARExpeditionSite : public AActor
{
    GENERATED_BODY()
public:
    AARExpeditionSite();

    UPROPERTY(EditAnywhere, Category="Expedition") bool bTown = false;
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
    UPROPERTY(EditInstanceOnly, Category="Passage") FText Prompt;
    UPROPERTY(EditAnywhere, Category="Passage", meta=(MakeEditWidget)) FVector ArrivalOffset = FVector(210, 0, 102);

    bool CanUse(const AARPlayerCharacter* Player) const;
    bool Use(AARPlayerCharacter* Player) const;
};
