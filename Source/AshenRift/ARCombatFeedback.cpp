#include "ARCombatFeedback.h"
#include "ARPrototype.h"
#include "ARProgression.h"
#include "AREnemyVariants.h"
#include "ARPreviewAnimInstance.h"
#include "ARHealthComponent.h"
#include "ARRunGameInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

UARFeedbackSubsystem* UARFeedbackSubsystem::Get(const UObject* Context)
{
    auto* Game = Context ? UGameplayStatics::GetGameInstance(Context) : nullptr;
    return Game ? Game->GetSubsystem<UARFeedbackSubsystem>() : nullptr;
}
bool UARFeedbackSubsystem::UsesUpdatedCombat()
{
#if WITH_DEV_AUTOMATION_TESTS
    const TCHAR* CommandLine = FCommandLine::Get();
    return !(FParse::Param(CommandLine, TEXT("ARTestProfile")) &&
        FParse::Param(CommandLine, TEXT("ARLegacyCombatTest")));
#else
    return true;
#endif
}
void UARFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    for (int32 Index = 0; Index <= int32(EARFeedbackCue::Charge); ++Index)
    {
        const FString Name = TEXT("S_AR_") + StaticEnum<EARFeedbackCue>()->GetNameStringByValue(Index);
        Sounds.Add(EARFeedbackCue(Index), LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/AshenRift/Audio/%s.%s"), *Name, *Name)));
    }
    for (int32 Index = 0; Index <= int32(EARCombatAction::Whirlwind); ++Index)
    {
        const FString Name = TEXT("A_AR_") + StaticEnum<EARCombatAction>()->GetNameStringByValue(Index);
        Animations.Add(EARCombatAction(Index), LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/AshenRift/Animations/%s.%s"), *Name, *Name)));
    }
    if (UsesUpdatedCombat())
    {
        const auto LoadEnemyClip = [this](EARCombatAction Action, const TCHAR* Name)
        {
            EnemyAnimations.Add(Action, LoadObject<UAnimSequence>(nullptr,
                *FString::Printf(TEXT("/Game/AshenRift/Animations/A_AR_%s.A_AR_%s"), Name, Name)));
        };
        LoadEnemyClip(EARCombatAction::Slash, TEXT("RaiderSwing"));
        LoadEnemyClip(EARCombatAction::Cast, TEXT("HexerCast"));
        LoadEnemyClip(EARCombatAction::Heavy, TEXT("SentinelShieldJab"));
        LoadEnemyClip(EARCombatAction::Hurt, TEXT("EnemyRecoil"));
        LoadEnemyClip(EARCombatAction::BossMelee, TEXT("WardenSweep"));
        LoadEnemyClip(EARCombatAction::BossArea, TEXT("WardenGroundSlam"));
        LoadEnemyClip(EARCombatAction::RaiderChop, TEXT("RaiderChop"));
        LoadEnemyClip(EARCombatAction::BossElbow, TEXT("WardenElbow"));
        Fall = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AshenRift/Animations/A_AR_EnemyStaggerFall.A_AR_EnemyStaggerFall"));
        BossFall = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AshenRift/Animations/A_AR_WardenKneelFall.A_AR_WardenKneelFall"));
        const TCHAR* MeshNames[] = {TEXT("SK_Raider"), TEXT("SK_Hexer"), TEXT("SK_Sentinel"), TEXT("SK_Warden")};
        for (const TCHAR* Name : MeshNames)
            KayKitMeshes.Add(Name, LoadObject<USkeletalMesh>(nullptr,
                *FString::Printf(TEXT("/Game/AshenRift/Enemies/KayKit/%s.%s"), Name, Name)));
        const TCHAR* ClipNames[] = {
            TEXT("Idle_Combat"), TEXT("Walking_A"), TEXT("1H_Melee_Attack_Chop"), TEXT("1H_Melee_Attack_Slice_Horizontal"),
            TEXT("Block_Attack"), TEXT("Spellcast_Shoot"), TEXT("Hit_A"), TEXT("Death_A"), TEXT("Death_C_Skeletons"),
            TEXT("2H_Melee_Attack_Chop"), TEXT("2H_Melee_Attack_Slice"), TEXT("Unarmed_Melee_Attack_Punch_B")
        };
        bKayKitClipsLoaded = true;
        for (const TCHAR* Name : ClipNames)
        {
            auto* Clip = LoadObject<UAnimSequence>(nullptr, *FString::Printf(
                TEXT("/Game/AshenRift/Enemies/KayKit/Animations/A_%s.A_%s"), Name, Name));
            KayKitClips.Add(Name, Clip);
            bKayKitClipsLoaded &= IsValid(Clip);
        }
    }
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(600);
    Attenuation->Attenuation.FalloffDistance = 1800;
}
void UARFeedbackSubsystem::Deinitialize()
{
    SetSoundEnabled(false);
    Sounds.Empty();
    Animations.Empty();
    EnemyAnimations.Empty();
    KayKitMeshes.Empty();
    KayKitClips.Empty();
    bKayKitClipsLoaded = false;
    Fall = nullptr;
    BossFall = nullptr;
    Super::Deinitialize();
}
void UARFeedbackSubsystem::SetSoundEnabled(bool bEnabled)
{
    bSoundEnabled = bEnabled;
    if (!bEnabled) { for (const auto& Voice : Voices) if (Voice.IsValid()) Voice->Stop(); Voices.Empty(); }
}
bool UARFeedbackSubsystem::PlayCue(EARFeedbackCue Cue, FVector Location, bool bUI)
{
    USoundBase* Sound = CueAsset(Cue);
    if (!bSoundEnabled || !Sound || !GetWorld()) return false;
    const double Now = FPlatformTime::Seconds();
    const double* Last = LastCueAt.Find(Cue);
    if (Last && Now - *Last < .025) return false;
    LastCueAt.Add(Cue, Now);
    Voices.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Voice) { return !Voice.IsValid() || !Voice->IsPlaying(); });
    if (Voices.Num() >= 12) { if (Voices[0].IsValid()) Voices[0]->Stop(); Voices.RemoveAt(0); }
    UAudioComponent* Voice = bUI ? UGameplayStatics::SpawnSound2D(this, Sound, .42f) :
        UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, .55f, 1, 0, Attenuation);
    if (Voice) Voices.Add(Voice);
    return Voice != nullptr;
}

UARCombatFeedbackComponent::UARCombatFeedbackComponent() { PrimaryComponentTick.bCanEverTick = true; }
void UARCombatFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Character = Cast<ACharacter>(GetOwner());
    Body = Character ? Character->GetMesh() : nullptr;
    if (Body)
    {
        Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (auto* Enemy = Cast<AAREnemy>(Character)) ConfigureKayKitEnemy(Enemy);
        if (!UsesKayKitEnemy())
            if (auto* AnimationClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/AshenRift/Animations/ABP_ARCombat.ABP_ARCombat_C"))) Body->SetAnimInstanceClass(AnimationClass);
    }
    FlashMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_ARHitFlash.M_ARHitFlash"));
    Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
    if (Camera) CameraRest = Camera->GetRelativeLocation();
    if (auto* Health = GetOwner()->FindComponentByClass<UARHealthComponent>())
    {
        PreviousHealth = Health->CurrentHealth;
        Health->OnHealthChanged.AddDynamic(this, &UARCombatFeedbackComponent::HealthChanged);
        Health->OnDamaged.AddDynamic(this, &UARCombatFeedbackComponent::Damaged);
        Health->OnDied.AddDynamic(this, &UARCombatFeedbackComponent::Died);
    }
}
void UARCombatFeedbackComponent::HealthChanged(float Current, float Maximum)
{ LastDamage = FMath::Max(0.f, PreviousHealth - Current); PreviousHealth = Current; }

void UARCombatFeedbackComponent::ConfigureKayKitEnemy(AAREnemy* Enemy)
{
    const auto* Assets = UARFeedbackSubsystem::Get(this);
    if (!Assets || !Assets->HasKayKitClips() || !Body) return;
    FName MeshName = TEXT("SK_Raider");
    const bool bBoss = Enemy->IsA<AARBoss>();
    if (bBoss) MeshName = TEXT("SK_Warden");
    else if (Enemy->IsA<AAREliteEnemy>()) MeshName = TEXT("SK_Sentinel");
    else if (Enemy->IsA<AARRangedEnemy>()) MeshName = TEXT("SK_Hexer");
    auto* Model = Assets->KayKitMesh(MeshName);
    auto* Idle = Assets->KayKitClip(TEXT("Idle_Combat"));
    auto* Walk = Assets->KayKitClip(TEXT("Walking_A"));
    if (!Model || Idle->GetSkeleton() != Model->GetSkeleton() || Walk->GetSkeleton() != Model->GetSkeleton()) return;

    Body->SetAnimInstanceClass(nullptr);
    Body->SetSkeletalMesh(Model);
    Body->EmptyOverrideMaterials();
    Body->SetAnimInstanceClass(UARPreviewAnimInstance::StaticClass());
    const FBoxSphereBounds Bounds = Model->GetImportedBounds();
    const float Scale = (bBoss ? 208.f : 176.f) / FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f);
    Body->SetRelativeScale3D(FVector(Scale));
    Body->SetRelativeLocation(FVector(0, 0, -Enemy->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() -
        (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
    const FReferenceSkeleton& Rig = Model->GetRefSkeleton();
    const int32 Foot = Rig.FindBoneIndex(TEXT("foot_r"));
    const int32 Toe = Rig.FindBoneIndex(TEXT("toes_r"));
    if (Foot != INDEX_NONE && Toe != INDEX_NONE)
    {
        const FVector Forward = FAnimationRuntime::GetComponentSpaceTransformRefPose(Rig, Toe).GetLocation() -
            FAnimationRuntime::GetComponentSpaceTransformRefPose(Rig, Foot).GetLocation();
        Body->SetRelativeRotation(FRotator(0, -Forward.Rotation().Yaw, 0));
    }
    KayKitIdle = Idle;
    KayKitWalk = Walk;
    UpdateKayKitLocomotion();
}

void UARCombatFeedbackComponent::UpdateKayKitLocomotion()
{
    const auto* Enemy = Cast<AAREnemy>(GetOwner());
    auto* Anim = Cast<UARPreviewAnimInstance>(Body->GetAnimInstance());
    if (!Enemy || !Anim || Enemy->Health->bDead) return;
    const auto* Current = Anim->GetAnimationAsset();
    if (Current && Current != KayKitIdle && Current != KayKitWalk && Anim->IsPlaying()) return;
    auto* Next = Enemy->GetVelocity().SizeSquared2D() > 225.f ? KayKitWalk : KayKitIdle;
    if (Current != Next) Anim->BlendTo(Next, true, 1.f, .12f);
}

bool UARCombatFeedbackComponent::PlayKayKitClip(FName Name, float Duration, float ImpactFraction)
{
    const auto* Assets = UARFeedbackSubsystem::Get(this);
    auto* Anim = Cast<UARPreviewAnimInstance>(Body->GetAnimInstance());
    auto* Clip = Assets ? Assets->KayKitClip(Name) : nullptr;
    if (!Anim || !Clip) return false;
    const float Rate = Duration > .01f ? Clip->GetPlayLength() * ImpactFraction / Duration : 1.f;
    Anim->BlendTo(Clip, false, Rate, .075f);
    return true;
}

bool UARCombatFeedbackComponent::PlayAction(EARCombatAction Action, float Duration)
{
    auto* State = Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(this));
    auto* Audio = UARFeedbackSubsystem::Get(this);
    auto* Health = GetOwner()->FindComponentByClass<UARHealthComponent>();
    if (!State || (State->Phase != EARRunPhase::InDungeon && State->Phase != EARRunPhase::Extracting) ||
        !Health || Health->bDead || !Body || !Body->GetAnimInstance()) return false;
    if (UsesKayKitEnemy())
    {
        FName Clip;
        switch (Action)
        {
        case EARCombatAction::Slash: Clip = TEXT("1H_Melee_Attack_Slice_Horizontal"); break;
        case EARCombatAction::RaiderChop: Clip = TEXT("1H_Melee_Attack_Chop"); break;
        case EARCombatAction::Cast: Clip = TEXT("Spellcast_Shoot"); break;
        case EARCombatAction::Heavy: Clip = TEXT("Block_Attack"); break;
        case EARCombatAction::Hurt: Clip = TEXT("Hit_A"); break;
        case EARCombatAction::BossMelee: Clip = TEXT("2H_Melee_Attack_Slice"); break;
        case EARCombatAction::BossArea: Clip = TEXT("2H_Melee_Attack_Chop"); break;
        case EARCombatAction::BossElbow: Clip = TEXT("Unarmed_Melee_Attack_Punch_B"); break;
        default: return false;
        }
        if (!PlayKayKitClip(Clip, Duration, .64f)) return false;
        LastAction = Action;
        return true;
    }
    const bool bBossAction = Action == EARCombatAction::BossMelee || Action == EARCombatAction::BossArea || Action == EARCombatAction::BossElbow;
    const bool bMonster = GetOwner()->IsA<AAREnemy>() && !GetOwner()->IsA<AARBoss>();
    const auto BaseAction = bBossAction ? EARCombatAction::Heavy : Action == EARCombatAction::RaiderChop ? EARCombatAction::Slash : Action;
    auto* EnemyClip = Audio && (bMonster || bBossAction) ? Audio->EnemyAnimation(Action) : nullptr;
    auto* Animation = EnemyClip ? EnemyClip : Audio ? Audio->Animation(BaseAction) : nullptr;
    if (!Animation) return false;
    const float ImpactFraction = EnemyClip && Action != EARCombatAction::Hurt ? .64f : 1.f;
    const float Rate = Duration > .01f ? Animation->SequenceLength * ImpactFraction / Duration : 1.f;
    auto* Montage = Body->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Animation, TEXT("DefaultSlot"),
        EnemyClip ? .085f : .035f, EnemyClip ? .115f : .06f, Rate);
    if (!Montage) return false;
    LastAction = Action;
    return true;
}
float UARCombatFeedbackComponent::PlayEnemyFall()
{
    if (UsesKayKitEnemy())
    {
        const bool bBoss = GetOwner()->IsA<AARBoss>();
        const float Duration = bBoss ? 1.85f : 1.25f;
        return PlayKayKitClip(bBoss ? TEXT("Death_A") : TEXT("Death_C_Skeletons"), Duration, 1.f) ? Duration : 0.f;
    }
    const auto* Audio = UARFeedbackSubsystem::Get(this);
    auto* Fall = Audio ? Audio->EnemyFall(GetOwner()->IsA<AARBoss>()) : nullptr;
    if (Fall && Body && Body->GetAnimInstance() && Body->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(
        Fall, TEXT("DefaultSlot"), .13f, 0.f))
        return Fall->SequenceLength - .035f;
    return 0.f;
}
void UARCombatFeedbackComponent::Damaged()
{
    auto* State = Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (!State || (State->Phase != EARRunPhase::InDungeon && State->Phase != EARRunPhase::Extracting)) return;
    const bool bPlayer = Cast<AARPlayerCharacter>(GetOwner()) != nullptr;
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(bPlayer ? EARFeedbackCue::Hurt : EARFeedbackCue::Hit, GetOwner()->GetActorLocation());
    AARImpactEffect::Spawn(this, GetOwner()->GetActorLocation() + FVector(0, 0, 40), LastDamage,
        bPlayer ? FLinearColor(1, .16f, .08f) : FLinearColor(1, .72f, .15f));
    const auto* Enemy = Cast<AAREnemy>(GetOwner());
    const auto* Audio = UARFeedbackSubsystem::Get(this);
    const auto* Boss = Cast<AARBoss>(Enemy);
    const bool bKeepTelegraph = Enemy && (Enemy->bWindingAttack || (Boss && Boss->bCasting)) &&
        (UsesKayKitEnemy() || (Audio && Audio->EnemyAnimation(EARCombatAction::Hurt)));
    if (!bKeepTelegraph) PlayAction(EARCombatAction::Hurt);
    if (Body && FlashMaterial)
    {
        if (!bFlashing)
        {
            RestoreMaterials.Empty();
            for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index) RestoreMaterials.Add(Body->GetMaterial(Index));
        }
        for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index) Body->SetMaterial(Index, FlashMaterial);
        bFlashing = true; FlashUntil = GetWorld()->GetTimeSeconds() + .12f;
    }
    if (Camera && bCameraFeedback) ShakeUntil = GetWorld()->GetTimeSeconds() + .16f;
}
void UARCombatFeedbackComponent::Died()
{ if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Death, GetOwner()->GetActorLocation()); }
void UARCombatFeedbackComponent::RestoreFlash()
{
    if (Body && bFlashing) for (int32 Index = 0; Index < RestoreMaterials.Num(); ++Index) Body->SetMaterial(Index, RestoreMaterials[Index]);
    bFlashing = false; RestoreMaterials.Empty();
}
void UARCombatFeedbackComponent::ResetFeedback()
{
    RestoreFlash(); ShakeUntil = 0;
    if (Camera) Camera->SetRelativeLocation(CameraRest);
    if (Body && Body->GetAnimInstance()) Body->GetAnimInstance()->StopAllMontages(0);
}
void UARCombatFeedbackComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaSeconds, TickType, Function);
    const float Now = GetWorld()->GetTimeSeconds();
    if (bFlashing && Now >= FlashUntil) RestoreFlash();
    if (UsesKayKitEnemy()) UpdateKayKitLocomotion();
    if (Camera)
    {
        const float Weight = FMath::Clamp((ShakeUntil - Now) / .16f, 0.f, 1.f);
        Camera->SetRelativeLocation(CameraRest + FVector(0, FMath::Sin(Now * 120) * 3.5f, FMath::Cos(Now * 95) * 2) * Weight);
    }
}
void UARCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason) { RestoreFlash(); Super::EndPlay(Reason); }

AARImpactEffect::AARImpactEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Number = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DamageNumber"));
    Number->SetupAttachment(RootComponent); Number->SetWorldSize(32); Number->SetHorizontalAlignment(EHTA_Center);
    Number->SetRelativeLocation(FVector(0, 0, 75)); Number->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (int32 Index = 0; Index < 6; ++Index)
    {
        auto* Shard = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Shard%d"), Index));
        Shard->SetupAttachment(RootComponent); Shard->SetStaticMesh(Cube.Object); Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shard->SetCanEverAffectNavigation(false); Shard->SetCastShadow(false); Shards.Add(Shard);
    }
    InitialLifeSpan = .55f;
}
void AARImpactEffect::BeginPlay()
{
    Super::BeginPlay();
    Number->SetText(FText::AsNumber(FMath::RoundToInt(DamageNumber))); Number->SetTextRenderColor(Color.ToFColor(true));
    Number->SetVisibility(DamageNumber > 0);
    auto* Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_ARImpact.M_ARImpact"));
    if (Source)
    {
        auto* Material = UMaterialInstanceDynamic::Create(Source, this); Material->SetVectorParameterValue(TEXT("Color"), Color);
        for (auto* Shard : Shards) Shard->SetMaterial(0, Material);
    }
}
void AARImpactEffect::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); Age += DeltaSeconds;
    for (int32 Index = 0; Index < Shards.Num(); ++Index)
    {
        const float Angle = Index * PI / 3;
        Shards[Index]->SetRelativeLocation(FVector(FMath::Cos(Angle), FMath::Sin(Angle), .4f) * Age * 170);
        Shards[Index]->SetRelativeRotation(FRotator(Age * 250, Index * 60, Age * 180));
        Shards[Index]->SetRelativeScale3D(FVector(.04f, .04f, .16f) * FMath::Max(0.f, 1 - Age / .4f));
    }
    Number->SetRelativeLocation(FVector(0, 0, 75 + Age * 80));
    if (auto* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Number->SetWorldRotation((CameraManager->GetCameraLocation() - Number->GetComponentLocation()).Rotation());
}
AARImpactEffect* AARImpactEffect::Spawn(const UObject* Context, FVector Location, float Damage, FLinearColor InColor)
{
    if (!Context || !Context->GetWorld()) return nullptr;
    int32 Count = 0; for (TActorIterator<AARImpactEffect> It(Context->GetWorld()); It; ++It) ++Count;
    if (Count >= 24) return nullptr;
    const FTransform Transform(Location);
    auto* Effect = Context->GetWorld()->SpawnActorDeferred<AARImpactEffect>(StaticClass(), Transform);
    if (Effect) { Effect->DamageNumber = Damage; Effect->Color = InColor; UGameplayStatics::FinishSpawningActor(Effect, Transform); }
    return Effect;
}
