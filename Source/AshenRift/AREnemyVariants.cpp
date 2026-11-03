#include "AREnemyVariants.h"
#include "ARHealthComponent.h"
#include "ARRunGameInstance.h"
#include "ARCombatFeedback.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    void ApplyBodyMaterial(AAREnemy* Enemy, const TCHAR* Path)
    {
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, Path))
            for (int32 Slot = 0; Slot < Enemy->GetMesh()->GetNumMaterials(); ++Slot) Enemy->GetMesh()->SetMaterial(Slot, Material);
    }
}
AAREnemyBolt::AAREnemyBolt()
{
    PrimaryActorTick.bCanEverTick = true;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent = Collision;
    Collision->InitSphereRadius(14);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Collision->SetCanEverAffectNavigation(false);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(.34f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->UpdatedComponent = Collision;
    Movement->InitialSpeed = Movement->MaxSpeed = 850;
    Movement->ProjectileGravityScale = 0;
    Movement->bRotationFollowsVelocity = true;
    Movement->Velocity = FVector(1, 0, 0);
    InitialLifeSpan = 3;
}
void AAREnemyBolt::BeginPlay()
{
    Super::BeginPlay();
    Collision->IgnoreActorWhenMoving(GetOwner(), true);
    Collision->OnComponentHit.AddDynamic(this, &AAREnemyBolt::Hit);
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Arcane.M_Arcane"))) Mesh->SetMaterial(0, Material);
}
bool AAREnemyBolt::IsAttackActive() const
{
    auto* Enemy = Cast<AAREnemy>(GetOwner());
    auto* State = Cast<UARRunGameInstance>(GetGameInstance());
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    return IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && !Enemy->Health->bDead && State && Player && Player->CanPlay() &&
        (State->Phase == EARRunPhase::InDungeon || State->Phase == EARRunPhase::Extracting);
}
void AAREnemyBolt::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsAttackActive()) Destroy();
}
void AAREnemyBolt::Hit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult&)
{
    if (bResolved || Other == GetOwner()) return;
    bResolved = true;
    if (IsAttackActive())
    {
        AARImpactEffect::Spawn(this, GetActorLocation(), 0, FLinearColor(.2f, .65f, 1));
        if (auto* Player = Cast<AARPlayerCharacter>(Other))
            UGameplayStatics::ApplyDamage(Player, Damage, GetInstigatorController(), this, nullptr);
    }
    Destroy();
}

AARRangedEnemy::AARRangedEnemy()
{
    DisplayName = TEXT("HEXER");
    Health->MaxHealth = 45;
    Damage = 18;
    GoldReward = 12;
    AttackInterval = 2.4f;
    AttackWindup = .8f;
    GetCharacterMovement()->MaxWalkSpeed = 160;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Staff = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Staff"));
    Staff->SetupAttachment(GetMesh(), TEXT("hand_r"));
    Staff->SetStaticMesh(Cube.Object);
    Staff->SetRelativeLocation(FVector(0, 0, 35));
    Staff->SetRelativeScale3D(FVector(.09f, .09f, 1.2f));
    Staff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Staff->SetCanEverAffectNavigation(false);
    AimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLine"));
    AimLine->SetupAttachment(RootComponent);
    AimLine->SetStaticMesh(Cube.Object);
    AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AimLine->SetCanEverAffectNavigation(false);
    AimLine->SetVisibility(false);
    AimLine->SetCastShadow(false);
}
void AARRangedEnemy::BeginPlay()
{
    Super::BeginPlay();
    ApplyBodyMaterial(this, TEXT("/Game/AshenRift/Materials/M_Hexer.M_Hexer"));
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Arcane.M_Arcane"))) Staff->SetMaterial(0, Material);
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Threat.M_Threat"))) AimLine->SetMaterial(0, Material);
}
void AARRangedEnemy::UpdateAimLine()
{
    const FVector Origin = GetActorLocation() + FVector(0, 0, 15);
    const FVector Delta = LockedTarget - Origin;
    AimLine->SetWorldLocation(Origin + Delta * .5f);
    AimLine->SetWorldRotation(Delta.Rotation());
    AimLine->SetWorldScale3D(FVector(Delta.Size() / 100, .035f, .035f));
    AimLine->SetVisibility(true);
}
void AARRangedEnemy::FireBolt()
{
    const FVector Origin = GetActorLocation() + FVector(0, 0, 15);
    const FRotator Aim = (LockedTarget - Origin).Rotation();
    const FTransform Transform(Aim, Origin + Aim.Vector() * 75);
    auto* Bolt = GetWorld()->SpawnActorDeferred<AAREnemyBolt>(AAREnemyBolt::StaticClass(), Transform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (Bolt) { Bolt->Damage = Damage; UGameplayStatics::FinishSpawningActor(Bolt, Transform); if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Cast, Origin); }
}
void AARRangedEnemy::Tick(float DeltaSeconds)
{
    // Avoid the base melee timer: this variant has a locked, dodgeable ranged attack.
    ACharacter::Tick(DeltaSeconds);
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Health->bDead || !Player || !Player->CanPlay())
    {
        bHasTarget = bWindingAttack = false;
        AimLine->SetVisibility(false);
        GetCharacterMovement()->StopMovementImmediately();
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (bWindingAttack)
    {
        if (Now >= HitAt)
        {
            bWindingAttack = false;
            AimLine->SetVisibility(false);
            if (ClearSight(Player)) FireBolt();
        }
        return;
    }
    if (!CanPursue(Player)) return;
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    if (Offset.Size() > FireRange || !ClearSight(Player)) { Pursue(Player); return; }
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0, Offset.Rotation().Yaw, 0));
    if (Now >= NextAttackAt)
    {
        LockedTarget = Player->GetActorLocation() + FVector(0, 0, 10);
        bWindingAttack = true;
        HitAt = Now + FMath::Max(.3f, AttackWindup);
        NextAttackAt = HitAt + FMath::Max(.5f, AttackInterval);
        UpdateAimLine();
        Feedback->PlayAction(EARCombatAction::Cast, AttackWindup);
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Charge, GetActorLocation());
        Player->ShowNotice(TEXT("HEXER AIMING - move sideways to dodge the bolt"));
    }
}

AAREliteEnemy::AAREliteEnemy()
{
    DisplayName = TEXT("SENTINEL");
    Health->MaxHealth = 120;
    Damage = 20;
    GoldReward = 20;
    RuneReward = 1;
    AttackInterval = 2;
    AttackWindup = .9f;
    MeleeReach = 250;
    StopRange = 200;
    AttackFacingDot = .35f;
    GetCharacterMovement()->MaxWalkSpeed = 145;
    SetActorScale3D(FVector(1.15f));
    Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
    Shield->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Shield->SetStaticMesh(Cube.Object);
    Shield->SetRelativeLocation(FVector(40, -50, 10));
    Shield->SetRelativeScale3D(FVector(.15f, .65f, .9f));
    Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Shield->SetCanEverAffectNavigation(false);
}
void AAREliteEnemy::BeginPlay()
{
    Super::BeginPlay();
    ApplyBodyMaterial(this, TEXT("/Game/AshenRift/Materials/M_Sentinel.M_Sentinel"));
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Sentinel.M_Sentinel"))) Shield->SetMaterial(0, Material);
}
