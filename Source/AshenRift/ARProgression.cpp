#include "ARProgression.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "AREquipmentComponent.h"
#include "ARCombatFeedback.h"
#include "ARRunGameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UARRunGameInstance* GetRun(const UObject* Object)
    { return Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(Object)); }
}

bool AARPlayerCharacter::SelectClass(EARPlayerClass Class)
{
    auto* State = GetRun(this);
    if (!State || !IsAtCamp() || (Class != EARPlayerClass::Warrior && Class != EARPlayerClass::Mage)) return false;
    SelectedClass = Class;
    State->MatchLoadoutToClass(this);
    ResetCombatForRun();
    ShowNotice(Class == EARPlayerClass::Mage ? TEXT("Mage selected: ranged bolts, mana and Arcane Burst [F]") : TEXT("Warrior selected: sword and Whirlwind [F]"));
    return true;
}
void AARPlayerCharacter::ResetCombatForRun()
{
    Feedback->ResetFeedback();
    Equipment->ClearEquipment();
    Health->MaxHealth = BaseMaxHealth();
    Health->ResetHealth();
    Mana = 100;
    RunPowerBonus = 0;
    NextSkillTime = NextAttackTime = ExtractUntil = 0;
    bShopOpen = false;
    ActiveShop = nullptr;
    Sword->SetRelativeScale3D(SelectedClass == EARPlayerClass::Mage ? FVector(.06f, .06f, 1.1f) : FVector(.07f, .07f, .85f));
    Sword->SetRelativeRotation(FRotator::ZeroRotator);
}
void AARPlayerCharacter::OptionOne()
{
    if (bShopOpen && IsValid(ActiveShop)) ActiveShop->Purchase(this, 1);
    else SelectClass(EARPlayerClass::Warrior);
}
void AARPlayerCharacter::OptionTwo()
{
    if (bShopOpen && IsValid(ActiveShop)) ActiveShop->Purchase(this, 2);
    else SelectClass(EARPlayerClass::Mage);
}
float AARPlayerCharacter::SkillRemaining() const
{ return FMath::Max(0.f, NextSkillTime - GetWorld()->GetTimeSeconds()); }
void AARPlayerCharacter::FireSpell(bool bHeavy)
{
    const float Cost = bHeavy ? 30.f : 8.f;
    if (Mana < Cost) { ShowNotice(TEXT("Not enough mana")); return; }
    const FRotator Aim(0, Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw, 0);
    SetActorRotation(Aim);
    const FTransform SpawnTransform(Aim, GetActorLocation() + Aim.Vector() * 85 + FVector(0, 0, 15));
    auto* Spell = GetWorld()->SpawnActorDeferred<AARSpell>(AARSpell::StaticClass(), SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Spell) return;
    Spell->Damage = (bHeavy ? 45.f : 18.f) + TotalDamageBonus();
    Spell->BurstRadius = bHeavy ? 230.f : 0.f;
    Mana -= Cost;
    UGameplayStatics::FinishSpawningActor(Spell, SpawnTransform);
    Feedback->PlayAction(EARCombatAction::Cast);
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Cast, GetActorLocation());
    if (bHeavy) NextSkillTime = GetWorld()->GetTimeSeconds() + 6;
}
void AARPlayerCharacter::UseSkill()
{
    auto* State = GetRun(this);
    if (!CanPlay() || IsUIBlockingInput() || !State || State->Phase != EARRunPhase::InDungeon || bShopOpen || SkillRemaining() > 0) return;
    if (SelectedClass == EARPlayerClass::Mage) { FireSpell(true); return; }
    NextSkillTime = GetWorld()->GetTimeSeconds() + 5;
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It)
    {
        if (It->Health->bDead || FVector::Dist(GetActorLocation(), It->GetActorLocation()) > 360) continue;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ARWhirlwind), false, this);
        if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), It->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == *It)
            UGameplayStatics::ApplyDamage(*It, 45 + TotalDamageBonus(), GetController(), this, nullptr);
    }
    const FTransform EffectTransform(FVector(GetActorLocation().X, GetActorLocation().Y, 8));
    auto* Effect = GetWorld()->SpawnActorDeferred<AARBlastZone>(AARBlastZone::StaticClass(), EffectTransform, this);
    if (Effect)
    {
        Effect->Radius = 360; Effect->Delay = .3f; Effect->bHurtsPlayer = false;
        UGameplayStatics::FinishSpawningActor(Effect, EffectTransform);
    }
    Feedback->PlayAction(EARCombatAction::Whirlwind);
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
    ShowNotice(TEXT("Whirlwind!"));
}

AARSpell::AARSpell()
{
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent = Collision;
    Collision->InitSphereRadius(16);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(.32f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->UpdatedComponent = Collision;
    Movement->InitialSpeed = 1300;
    Movement->MaxSpeed = 1300;
    Movement->ProjectileGravityScale = 0;
    Movement->bRotationFollowsVelocity = true;
    Movement->Velocity = FVector(1, 0, 0);
    auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
    Light->SetupAttachment(RootComponent);
    Light->SetIntensity(1500);
    Light->SetAttenuationRadius(220);
    Light->SetLightColor(FLinearColor(.1f, .45f, 1));
    InitialLifeSpan = 2.5f;
}
void AARSpell::BeginPlay()
{
    Super::BeginPlay();
    Collision->IgnoreActorWhenMoving(GetOwner(), true);
    // A quick basic + burst combo must not collide at their shared spawn point.
    for (TActorIterator<AARSpell> It(GetWorld()); It; ++It)
        if (*It != this && It->GetOwner() == GetOwner())
        { Collision->IgnoreActorWhenMoving(*It, true); It->Collision->IgnoreActorWhenMoving(this, true); }
    Collision->OnComponentHit.AddDynamic(this, &AARSpell::Hit);
    if (BurstRadius > 0) { Collision->SetSphereRadius(26); Mesh->SetRelativeScale3D(FVector(.52f)); }
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Arcane.M_Arcane"))) Mesh->SetMaterial(0, Material);
}
void AARSpell::Hit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult& Result)
{
    if (bResolved || Other == GetOwner()) return;
    bResolved = true;
    auto* Player = Cast<AARPlayerCharacter>(GetOwner());
    const auto* State = GetRun(this);
    if (Player && Player->CanPlay() && State && State->Phase == EARRunPhase::InDungeon)
    {
        AARImpactEffect::Spawn(this, Result.ImpactPoint, 0, FLinearColor(.15f, .65f, 1));
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(BurstRadius > 0 ? EARFeedbackCue::Blast : EARFeedbackCue::Hit, Result.ImpactPoint);
        // Other enemies do not shield a burst; only world geometry blocks it.
        FCollisionQueryParams CoverQuery(SCENE_QUERY_STAT(ARSpellBurst), false, this);
        CoverQuery.AddIgnoredActor(Player);
        for (TActorIterator<AAREnemy> CoverIt(GetWorld()); CoverIt; ++CoverIt) CoverQuery.AddIgnoredActor(*CoverIt);
        for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It)
        {
            if (It->Health->bDead) continue;
            const bool bDirect = *It == Other;
            if (!bDirect && (BurstRadius <= 0 || FVector::Dist(It->GetActorLocation(), Result.ImpactPoint) > BurstRadius)) continue;
            FHitResult CoverHit;
            if (bDirect || !GetWorld()->LineTraceSingleByChannel(CoverHit, Result.ImpactPoint + Result.ImpactNormal * 3, It->GetActorLocation(), ECC_Visibility, CoverQuery))
                UGameplayStatics::ApplyDamage(*It, Damage, Player->GetController(), this, nullptr);
        }
        Player->ShowNotice(TEXT("Arcane impact"));
    }
    Destroy();
}

AARBlastZone::AARBlastZone()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Mesh->SetStaticMesh(Cylinder.Object);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AARBlastZone::BeginPlay()
{
    Super::BeginPlay();
    ResolveAt = GetWorld()->GetTimeSeconds() + Delay;
    SetActorScale3D(FVector(Radius / 50, Radius / 50, .025f));
    const TCHAR* Path = bHurtsPlayer ? TEXT("/Game/AshenRift/Materials/M_Threat.M_Threat") : TEXT("/Game/AshenRift/Materials/M_Gold.M_Gold");
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, Path)) Mesh->SetMaterial(0, Material);
}
void AARBlastZone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* State = GetRun(this);
    if (!State || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || State->Phase != EARRunPhase::InDungeon)
    { Destroy(); return; }
    if (bResolved || GetWorld()->GetTimeSeconds() < ResolveAt) return;
    bResolved = true;
    AARImpactEffect::Spawn(this, GetActorLocation() + FVector(0, 0, 20), 0, bHurtsPlayer ? FLinearColor(1, .16f, .04f) : FLinearColor(1, .7f, .1f));
    if (bHurtsPlayer) if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Blast, GetActorLocation());
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (bHurtsPlayer && Player && Player->CanPlay() && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= Radius && FMath::Abs(Player->GetActorLocation().Z - GetActorLocation().Z) < 180)
    {
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ARBlastSight), false, this);
        Query.AddIgnoredActor(GetOwner());
        if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0, 0, 80), Player->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Player)
            UGameplayStatics::ApplyDamage(Player, Damage, nullptr, GetOwner(), nullptr);
    }
    Destroy();
}

AARBoss::AARBoss()
{
    Health->MaxHealth = 360;
    Damage = 18;
    GoldReward = 70;
    RuneReward = 2;
    DetectRange = 1400;
    GetCharacterMovement()->MaxWalkSpeed = 145;
    SetActorScale3D(FVector(1.5f));
}
void AARBoss::Tick(float DeltaSeconds)
{
    // Boss has its own cadence, separate from the ordinary enemy's melee timer.
    ACharacter::Tick(DeltaSeconds);
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Health->bDead || !Player || !Player->CanPlay())
    { bCasting = bWindingAttack = false; GetCharacterMovement()->StopMovementImmediately(); return; }
    const float Now = GetWorld()->GetTimeSeconds();
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    if (bCasting) { if (Now >= CastEndsAt) bCasting = false; return; }
    if (bWindingAttack)
    {
        if (Now >= HitAt)
        {
            bWindingAttack = false;
            if (Offset.Size() <= 250 && ClearSight(Player)) UGameplayStatics::ApplyDamage(Player, Damage, GetController(), this, nullptr);
        }
        return;
    }
    if (!CanPursue(Player)) return;
    if (!ClearSight(Player)) { Pursue(Player); return; }
    SetActorRotation(FRotator(0, Offset.Rotation().Yaw, 0));
    if (Now >= NextCastAt && Offset.Size() <= 1050)
    {
        GetCharacterMovement()->StopMovementImmediately();
        const FTransform Transform(FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y, 8));
        auto* Zone = GetWorld()->SpawnActorDeferred<AARBlastZone>(AARBlastZone::StaticClass(), Transform, this);
        if (Zone) UGameplayStatics::FinishSpawningActor(Zone, Transform);
        bCasting = true; CastEndsAt = Now + 1.2f; NextCastAt = Now + 4.5f;
        Feedback->PlayAction(EARCombatAction::Heavy, 1.2f);
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Charge, GetActorLocation());
        Player->ShowNotice(TEXT("WARDEN SLAM - leave the red circle!"));
    }
    else if (Offset.Size() > 210) Pursue(Player);
    else if (Now >= NextAttackAt)
    { bWindingAttack = true; HitAt = Now + .65f; NextAttackAt = Now + 1.6f; Feedback->PlayAction(EARCombatAction::Heavy, .65f); }
}
void AARBoss::Died()
{
    if (auto* State = GetRun(this)) { State->bBossDefeated = true; State->OnChanged.Broadcast(); }
    Super::Died();
}

AARShop::AARShop()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetRelativeScale3D(FVector(2.4f, 1.2f, 1.1f));
}
bool AARShop::CanTrade(AARPlayerCharacter* Player) const
{
    const auto* State = GetRun(this);
    if (!Player || !Player->CanPlay() || !State || State->Phase != EARRunPhase::InDungeon || FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) > 280) return false;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ARShopSight), false, this);
    return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0, 0, 120), Player->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Player;
}
bool AARShop::Purchase(AARPlayerCharacter* Player, int32 Offer)
{
    if (bProcessing || !CanTrade(Player) || !Player->bShopOpen || Player->ActiveShop != this) return false;
    TGuardValue<bool> Guard(bProcessing, true);
    auto* State = GetRun(this);
    bool bSuccess = false;
    if (Offer == 1 && PotionsSold < 5) { bSuccess = State->BuyPotion(Player->Inventory); if (bSuccess) ++PotionsSold; }
    if (Offer == 2 && UpgradesSold < 3) { bSuccess = State->BuyPower(Player); if (bSuccess) ++UpgradesSold; }
    Player->ShowNotice(bSuccess ? TEXT("Purchase complete") : TEXT("Cannot buy: check gold, stock or inventory space"));
    return bSuccess;
}
void AARShop::ResetStock() { PotionsSold = UpgradesSold = 0; }
