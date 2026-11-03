#pragma once
#include "CoreMinimal.h"
#include "ARPrototype.h"
#include "ARProgression.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS()
class ASHENRIFT_API AARSpell : public AActor
{
    GENERATED_BODY()
public:
    AARSpell();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere) USphereComponent* Collision;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 18;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BurstRadius = 0;
private:
    bool bResolved = false;
    UFUNCTION() void Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector Impulse, const FHitResult& Result);
};

// Fixed telegraph: the player can step outside before the delayed hit.
UCLASS()
class ASHENRIFT_API AARBlastZone : public AActor
{
    GENERATED_BODY()
public:
    AARBlastZone();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 280;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Delay = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHurtsPlayer = true;
private:
    float ResolveAt = 0;
    bool bResolved = false;
};

UCLASS()
class ASHENRIFT_API AARBoss : public AAREnemy
{
    GENERATED_BODY()
public:
    AARBoss();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly) bool bCasting = false;
protected:
    virtual void Died() override;
private:
    float CastEndsAt = 0;
    float NextCastAt = 0;
};

UCLASS()
class ASHENRIFT_API AARShop : public AActor
{
    GENERATED_BODY()
public:
    AARShop();
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(BlueprintReadOnly) int32 PotionsSold = 0;
    UPROPERTY(BlueprintReadOnly) int32 UpgradesSold = 0;
    UFUNCTION(BlueprintPure) bool CanTrade(AARPlayerCharacter* Player) const;
    UFUNCTION(BlueprintCallable) bool Purchase(AARPlayerCharacter* Player, int32 Offer);
    void ResetStock();
private:
    bool bProcessing = false;
};
