#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARExpeditionSite.generated.h"

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
