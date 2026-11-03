#pragma once
#include "CoreMinimal.h"
#include "ARPrototype.h"
#include "AREnemyVariants.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS()
class ASHENRIFT_API AAREnemyBolt : public AActor
{
    GENERATED_BODY()
public:
    AAREnemyBolt();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USphereComponent* Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UProjectileMovementComponent* Movement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 18;
private:
    bool bResolved = false;
    bool IsAttackActive() const;
    UFUNCTION() void Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent, FVector Impulse, const FHitResult& Result);
};

UCLASS()
class ASHENRIFT_API AARRangedEnemy : public AAREnemy
{
    GENERATED_BODY()
public:
    AARRangedEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float FireRange = 1050;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Staff;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* AimLine;
private:
    FVector LockedTarget;
    void FireBolt();
    void UpdateAimLine();
};

UCLASS()
class ASHENRIFT_API AAREliteEnemy : public AAREnemy
{
    GENERATED_BODY()
public:
    AAREliteEnemy();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Shield;
};
