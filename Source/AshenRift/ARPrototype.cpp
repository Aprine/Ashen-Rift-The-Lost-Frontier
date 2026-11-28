#include "ARPrototype.h"
#include "ARProgression.h"
#include "AREnemyVariants.h"
#include "ARDungeon.h"
#include "AREquipmentComponent.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "ARRunGameInstance.h"
#include "ARProfile.h"
#include "ARGameWidget.h"
#include "ARCombatFeedback.h"
#include "ARPreviewAnimInstance.h"
#include "ARExpeditionSite.h"
#include "ARBranchWing.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"

namespace
{
    constexpr float DodgeDuration = .24f;
    constexpr float DodgeSpeed = 1500.f;
    constexpr float DodgeImmunity = .18f;
    constexpr float DodgeCooldown = .95f;

    UARRunGameInstance* Run(const UObject* Object)
    { return Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(Object)); }
    void SetupMannequin(USkeletalMeshComponent* Mesh)
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
        static ConstructorHelpers::FClassFinder<UAnimInstance> Animation(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
        if (Body.Succeeded()) Mesh->SetSkeletalMesh(Body.Object);
        if (Animation.Succeeded()) Mesh->SetAnimInstanceClass(Animation.Class);
        Mesh->SetRelativeLocation(FVector(0, 0, -96));
        Mesh->SetRelativeRotation(FRotator(0, -90, 0));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

AARPlayerCharacter::AARPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42, 96);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 720, 0);
    GetCharacterMovement()->MaxWalkSpeed = 520;
    GetCharacterMovement()->JumpZVelocity = 500;
    SetupMannequin(GetMesh());
    Health = CreateDefaultSubobject<UARHealthComponent>(TEXT("Health"));
    Inventory = CreateDefaultSubobject<UARInventoryComponent>(TEXT("Inventory"));
    Equipment = CreateDefaultSubobject<UAREquipmentComponent>(TEXT("Equipment"));
    Feedback = CreateDefaultSubobject<UARCombatFeedbackComponent>(TEXT("CombatFeedback"));
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 440;
    CameraBoom->SocketOffset = FVector(0, 45, 75);
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom);
    Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
    Sword->SetupAttachment(GetMesh(), TEXT("hand_r"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Sword->SetStaticMesh(Cube.Object);
    Sword->SetRelativeScale3D(FVector(.07f, .07f, .85f));
    Sword->SetRelativeLocation(FVector(0, 0, 35));
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AARPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health->OnDied.AddDynamic(this, &AARPlayerCharacter::Died);
    Health->OnDamaged.AddDynamic(this, &AARPlayerCharacter::Damaged);
    if (const auto* State = Run(this)) SelectedClass = State->TravelClass;
    ResetCombatForRun();
    if (Controller)
    {
        const auto* Mode = GetWorld()->GetAuthGameMode<AARPrototypeGameMode>();
        Controller->SetControlRotation(FRotator(-15, Mode ? Mode->CampYaw() : 0, 0));
    }
    if (const auto* State = Run(this)) if (!State->LastError.IsEmpty()) ShowNotice(State->LastError);
}
void AARPlayerCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &AARPlayerCharacter::MoveForward);
    Input->BindAxis("MoveRight", this, &AARPlayerCharacter::MoveRight);
    Input->BindAxis("Turn", this, &AARPlayerCharacter::LookYaw);
    Input->BindAxis("LookUp", this, &AARPlayerCharacter::LookPitch);
    Input->BindAction("Jump", IE_Pressed, this, &AARPlayerCharacter::SafeJump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction("Attack", IE_Pressed, this, &AARPlayerCharacter::Attack);
    Input->BindAction("Dodge", IE_Pressed, this, &AARPlayerCharacter::Dodge);
    Input->BindAction("Interact", IE_Pressed, this, &AARPlayerCharacter::Interact);
    Input->BindAction("BranchMap", IE_Pressed, this, &AARPlayerCharacter::ToggleBranchMap);
    Input->BindAction("Potion", IE_Pressed, this, &AARPlayerCharacter::UsePotion);
    Input->BindAction("Return", IE_Pressed, this, &AARPlayerCharacter::ReturnOrRetry);
    Input->BindAction("Inventory", IE_Pressed, this, &AARPlayerCharacter::ToggleInventory);
    Input->BindAction("Skill", IE_Pressed, this, &AARPlayerCharacter::UseSkill);
    Input->BindAction("OptionOne", IE_Pressed, this, &AARPlayerCharacter::OptionOne);
    Input->BindAction("OptionTwo", IE_Pressed, this, &AARPlayerCharacter::OptionTwo);
    Input->BindAction("EquipWeapon", IE_Pressed, this, &AARPlayerCharacter::EquipWeapon);
    Input->BindAction("EquipArmor", IE_Pressed, this, &AARPlayerCharacter::EquipArmor);
    Input->BindAction("EquipCharm", IE_Pressed, this, &AARPlayerCharacter::EquipCharm);
    Input->BindAction("Camp", IE_Pressed, this, &AARPlayerCharacter::ToggleCamp);
    Input->BindAction("CampPrevious", IE_Pressed, this, &AARPlayerCharacter::CampPrevious);
    Input->BindAction("CampNext", IE_Pressed, this, &AARPlayerCharacter::CampNext);
    Input->BindAction("CampAdd", IE_Pressed, this, &AARPlayerCharacter::CampAdd);
    Input->BindAction("CampRemove", IE_Pressed, this, &AARPlayerCharacter::CampRemove);
    Input->BindAction("Vitality", IE_Pressed, this, &AARPlayerCharacter::BuyVitality);
    Input->BindAction("Might", IE_Pressed, this, &AARPlayerCharacter::BuyMight);
    Input->BindAction("Menu", IE_Pressed, this, &AARPlayerCharacter::TogglePauseMenu).bExecuteWhenPaused = true;
}
bool AARPlayerCharacter::CanPlay() const
{
    const auto* State = Run(this);
    return State && !Health->bDead && (State->Phase == EARRunPhase::InDungeon || State->Phase == EARRunPhase::Extracting);
}
void AARPlayerCharacter::MoveForward(float Value)
{
    const auto* State = Run(this);
    if (Controller && !IsDodging() && !IsUIBlockingInput() && !Health->bDead && State && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling)
        AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value);
}
void AARPlayerCharacter::MoveRight(float Value)
{
    const auto* State = Run(this);
    if (Controller && !IsDodging() && !IsUIBlockingInput() && !Health->bDead && State && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling)
        AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value);
}
void AARPlayerCharacter::SafeJump()
{ const auto* State = Run(this); if (State && !IsDodging() && !IsUIBlockingInput() && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling && !Health->bDead) Jump(); }
float AARPlayerCharacter::DodgeCooldownRemaining() const
{
    const auto* World = GetWorld();
    return World ? FMath::Max(0.f, NextDodgeTime - World->GetTimeSeconds()) : 0.f;
}

void AARPlayerCharacter::Dodge()
{
    auto* Movement = GetCharacterMovement();
    if (!CanPlay() || IsDodging() || IsUIBlockingInput() || bShopOpen || UGameplayStatics::IsGamePaused(this) ||
        DodgeCooldownRemaining() > 0.f || !Movement->IsMovingOnGround()) return;

    FVector Direction = GetPendingMovementInputVector().GetSafeNormal2D();
    const auto* PC = Cast<APlayerController>(Controller);
    if (PC && PC->PlayerInput)
    {
        // Actions run before the bound axis delegates. Read the current mapped keys for simultaneous presses.
        const auto ReadAxis = [PC](FName Axis)
        {
            float Value = 0.f;
            for (const FInputAxisKeyMapping& Mapping : PC->PlayerInput->GetKeysForAxis(Axis))
                Value += Mapping.Scale * PC->GetInputAnalogKeyState(Mapping.Key);
            return FMath::Clamp(Value, -1.f, 1.f);
        };
        const FRotationMatrix View(FRotator(0, PC->GetControlRotation().Yaw, 0));
        const FVector MappedDirection = View.GetUnitAxis(EAxis::X) * ReadAxis(TEXT("MoveForward")) +
            View.GetUnitAxis(EAxis::Y) * ReadAxis(TEXT("MoveRight"));
        if (!MappedDirection.IsNearlyZero(.1f)) Direction = MappedDirection.GetSafeNormal2D();
    }
    DodgeDirection = Direction.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Direction;
    DodgeTimeRemaining = DodgeDuration;
    NextDodgeTime = GetWorld()->GetTimeSeconds() + DodgeCooldown;
    Health->SetDamageImmunity(DodgeImmunity);

    ConsumeMovementInputVector();
    Movement->StopMovementImmediately();
    SetActorRotation(DodgeDirection.Rotation());
    bPreviewAttack = false;
    if (bPreviewCharacter) PlayPreviewClip(TEXT("Running_A"), true, 0.f, .055f);
    else Feedback->ResetFeedback();
}

void AARPlayerCharacter::UpdateDodge(float DeltaSeconds)
{
    if (!CanPlay())
    {
        DodgeTimeRemaining = 0.f;
        Health->SetDamageImmunity(0.f);
        return;
    }
    const float Step = FMath::Min(DodgeTimeRemaining, DeltaSeconds);
    DodgeTimeRemaining -= Step;
    auto* Movement = GetCharacterMovement();
    if (!Movement->IsMovingOnGround()) DodgeDirection = FVector::ZeroVector;
    if (DodgeDirection.IsNearlyZero()) return;
    FHitResult Hit;
    Movement->SafeMoveUpdatedComponent(DodgeDirection * (DodgeSpeed * Step), GetActorQuat(), true, Hit);
    Movement->bForceNextFloorCheck = true;
    if (Hit.IsValidBlockingHit()) DodgeDirection = FVector::ZeroVector;
}
void AARPlayerCharacter::ShowNotice(const FString& Text)
{ Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 2.5f; }
void AARPlayerCharacter::ToggleInventory() { bInventoryVisible = !bInventoryVisible; if (bInventoryVisible) bCampOpen = false; }
void AARPlayerCharacter::Attack()
{
    auto* State = Run(this);
    if (!CanPlay() || IsDodging() || IsUIBlockingInput() || bShopOpen || State->Phase != EARRunPhase::InDungeon || GetWorld()->GetTimeSeconds() < NextAttackTime) return;
    if (SelectedClass == EARPlayerClass::Mage && Mana < 8) { ShowNotice(TEXT("Not enough mana")); return; }
    if (bPreviewCharacter)
    {
        if (StartPreviewAction(SelectedClass == EARPlayerClass::Mage ? EARCombatAction::Cast : EARCombatAction::Slash, false))
            NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, AttackCooldown);
        return;
    }
    NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, AttackCooldown);
    if (SelectedClass == EARPlayerClass::Mage) { FireSpell(false); return; }
    Feedback->PlayAction(EARCombatAction::Slash);
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
    if (Controller) SetActorRotation(FRotator(0, Controller->GetControlRotation().Yaw, 0));
    ResolveSlash();
}
void AARPlayerCharacter::ResolveSlash()
{
    const FVector Origin = GetActorLocation();
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It)
    {
        AAREnemy* Enemy = *It;
        const FVector Offset = Enemy->GetActorLocation() - Origin;
        if (Enemy->Health->bDead || Offset.Size() > 215 || FVector::DotProduct(Offset.GetSafeNormal2D(), GetActorForwardVector()) < .25f) continue;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ARAttack), false, this);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Enemy->GetActorLocation(), ECC_Visibility, Query);
        if (!bBlocked || Hit.GetActor() == Enemy)
        {
            UGameplayStatics::ApplyDamage(Enemy, AttackDamage + TotalDamageBonus(), GetController(), this, nullptr);
            ShowNotice(TEXT("Hit!"));
        }
    }
}
bool AARPlayerCharacter::StartPreviewAction(EARCombatAction Action, bool bSkill)
{
    auto* Anim = Cast<UARPreviewAnimInstance>(GetMesh()->GetAnimInstance());
    if (!Anim || bPreviewAttack || (PreviewClip == PreviewClips.FindRef(TEXT("Hit_A")) &&
        Anim->IsPlaying())) return false;
    PreviewAction = Action;
    bPreviewSkill = bSkill;
    bPreviewAttack = true;
    bPreviewContact = false;
    PreviewAim = FRotator(0, Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw, 0);
    SetActorRotation(PreviewAim);
    if (Action == EARCombatAction::Cast) PlayPreviewClip(bSkill ? TEXT("Spellcast_Long") : TEXT("Spellcast_Shoot"), false, bSkill ? .8f : .48f, .075f);
    else PlayPreviewClip(bSkill ? TEXT("2H_Melee_Attack_Spin") : TEXT("1H_Melee_Attack_Slice_Horizontal"), false, bSkill ? .75f : .46f, .075f);
    Feedback->LastAction = Action;
    if (Action != EARCombatAction::Cast)
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
    return true;
}
void AARPlayerCharacter::PlayPreviewClip(FName Name, bool bLoop, float Duration, float BlendSeconds)
{
    auto* Clip = PreviewClips.FindRef(Name);
    auto* Anim = Cast<UARPreviewAnimInstance>(GetMesh()->GetAnimInstance());
    if (!Anim || !Clip || (bLoop && PreviewClip == Clip)) return;
    PreviewClip = Clip;
    Anim->BlendTo(Clip, bLoop, Duration > 0 ? Clip->GetPlayLength() / Duration : 1.f, BlendSeconds);
}

void AARPlayerCharacter::ConfigurePreviewCharacter()
{
    if (!UARFeedbackSubsystem::UsesUpdatedCombat()) return;
    const bool bMage = SelectedClass == EARPlayerClass::Mage;
    const FString ClassName = bMage ? TEXT("Mage") : TEXT("Knight");
    const FString Folder = TEXT("/Game/AshenRift/PlayerPreview/") + ClassName;
    auto* Model = LoadObject<USkeletalMesh>(nullptr, *(Folder / ClassName));
    if (!Model) return;
    TMap<FName, UAnimSequence*> Clips;
    const TCHAR* Names[] = {TEXT("Idle"), TEXT("Running_A"), TEXT("Jump_Start"), TEXT("Jump_Idle"), TEXT("Jump_Land"), TEXT("Hit_A"), TEXT("Death_A"),
        bMage ? TEXT("Spellcast_Shoot") : TEXT("1H_Melee_Attack_Slice_Horizontal"),
        bMage ? TEXT("Spellcast_Long") : TEXT("2H_Melee_Attack_Spin")};
    for (const TCHAR* Name : Names)
    {
        auto* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder / TEXT("Clips") / (ClassName + TEXT("_") + Name)));
        if (!Clip || Clip->GetSkeleton() != Model->GetSkeleton()) return;
        Clips.Add(FName(Name), Clip);
    }
    bPreviewCharacter = true;
    PreviewClips = MoveTemp(Clips);
    PreviewClip = nullptr;
    bPreviewWasFalling = false;
    if (!Cast<UARPreviewAnimInstance>(GetMesh()->GetAnimInstance()))
        GetMesh()->SetAnimInstanceClass(nullptr);
    GetMesh()->SetSkeletalMesh(Model, true);
    GetMesh()->SetAnimInstanceClass(UARPreviewAnimInstance::StaticClass());
    const FBoxSphereBounds BodyBounds = Model->GetImportedBounds();
    const float BodyScale = 175.f / FMath::Max(BodyBounds.BoxExtent.Z * 2.f, .01f);
    GetMesh()->SetRelativeScale3D(FVector(BodyScale));
    GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() -
        (BodyBounds.Origin.Z - BodyBounds.BoxExtent.Z) * BodyScale));
    const FReferenceSkeleton& Ref = Model->GetRefSkeleton();
    const int32 Foot = Ref.FindBoneIndex(TEXT("foot_r"));
    const int32 Toe = Ref.FindBoneIndex(TEXT("toes_r"));
    if (Foot != INDEX_NONE && Toe != INDEX_NONE)
    {
        const FVector Forward = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Toe).GetLocation() -
            FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Foot).GetLocation();
        GetMesh()->SetRelativeRotation(FRotator(0, -Forward.Rotation().Yaw, 0));
    }
    Sword->SetVisibility(false);
    PlayPreviewClip(TEXT("Idle"), true, 0.f, 0.f);
}
void AARPlayerCharacter::UpdatePreviewAction()
{
    auto* Anim = Cast<UARPreviewAnimInstance>(GetMesh()->GetAnimInstance());
    if (!Anim || !PreviewClip || Health->bDead) return;
    const auto* State = Run(this);
    const bool bFalling = GetCharacterMovement()->IsFalling();
    const bool bFallingChanged = bFalling != bPreviewWasFalling;
    bPreviewWasFalling = bFalling;
    if (bPreviewAttack)
    {
        if (!CanPlay() || State->Phase != EARRunPhase::InDungeon)
        {
            bPreviewAttack = false;
            return;
        }
        const float ContactFraction = PreviewAction == EARCombatAction::Cast ? .60f : .52f;
        if (!bPreviewContact && Anim->GetCurrentTime() >= PreviewClip->GetPlayLength() * ContactFraction)
        {
            bPreviewContact = true;
            SetActorRotation(PreviewAim);
            switch (PreviewAction)
            {
            case EARCombatAction::Slash: ResolveSlash(); break;
            case EARCombatAction::Cast: FireSpell(bPreviewSkill); break;
            case EARCombatAction::Whirlwind: ResolveWhirlwind(); break;
            default: break;
            }
        }
        if (!Anim->IsPlaying()) bPreviewAttack = false;
        return;
    }
    if (PreviewClip == PreviewClips.FindRef(TEXT("Hit_A")) && Anim->IsPlaying()) return;
    if (bFalling)
    {
        if (bFallingChanged && GetVelocity().Z > 0.f) PlayPreviewClip(TEXT("Jump_Start"), false, .20f, .095f);
        else if (PreviewClip != PreviewClips.FindRef(TEXT("Jump_Start")) || !Anim->IsPlaying())
            PlayPreviewClip(TEXT("Jump_Idle"), true, 0.f, .14f);
        return;
    }
    if (bFallingChanged)
    {
        PlayPreviewClip(TEXT("Jump_Land"), false, .22f, .095f);
        return;
    }
    if (PreviewClip == PreviewClips.FindRef(TEXT("Jump_Land")) && Anim->IsPlaying()) return;
    PlayPreviewClip(GetVelocity().SizeSquared2D() > 100 ? TEXT("Running_A") : TEXT("Idle"), true);
}
void AARPlayerCharacter::UsePotion()
{
    if (!CanPlay() || UGameplayStatics::IsGamePaused(this) || Inventory->Count(TEXT("Potion")) <= 0) return;
    if (Health->Heal(30) > 0)
    {
        Inventory->TryRemove(TEXT("Potion"), 1);
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Heal, GetActorLocation());
        ShowNotice(TEXT("Potion: restored health"));
    }
}
void AARPlayerCharacter::Interact()
{
    auto* State = Run(this);
    if (!State) return;
    auto* PC = Cast<APlayerController>(Controller);
    auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
    if (HUD && HUD->GameUI && HUD->GameUI->IsMenuOpen()) return;
    if (HUD && HUD->GameUI &&
        (HUD->GameUI->CurrentScreen() == EARUIScreen::Routes || HUD->GameUI->CurrentScreen() == EARUIScreen::BranchMap))
    {
        HUD->GameUI->CloseRouteBoard();
        return;
    }
    if (bShopOpen) { bShopOpen = false; ActiveShop = nullptr; return; }
    if (State->Phase == EARRunPhase::Hub)
    {
        if (!IsAtCamp())
        { ShowNotice(TEXT("Return to the camp to enter the dungeon")); return; }
        if (auto* Mode = Cast<AARPrototypeGameMode>(GetWorld()->GetAuthGameMode())) Mode->StartDungeon(this);
        return;
    }
    if (!CanPlay() || IsDodging() || State->Phase != EARRunPhase::InDungeon) return;
    for (TActorIterator<AARShop> It(GetWorld()); It; ++It)
    {
        if (It->CanTrade(this)) { ActiveShop = *It; bShopOpen = true; bInventoryVisible = false; return; }
    }
    for (TActorIterator<AARBranchCache> It(GetWorld()); It; ++It)
    {
        if (!It->IsNearby(this)) continue;
        It->Claim(this);
        return;
    }
    for (TActorIterator<AARExpeditionPassage> It(GetWorld()); It; ++It)
    {
        if (!It->CanUse(this)) continue;
        if (!It->Use(this)) ShowNotice(TEXT("暂时无法通过，出口被挡住了。"));
        return;
    }
    if (HUD && HUD->GameUI)
        for (TActorIterator<AARExpeditionRouteBoard> It(GetWorld()); It; ++It)
            if (HUD->GameUI->OpenRouteBoard(*It)) return;
    for (TActorIterator<AARPortal> It(GetWorld()); It; ++It)
    {
        if (!It->IsNearby(this)) continue;
        if (It->IsSealed()) { ShowNotice(It->Prompt()); return; }
        ExtractLocation = It->GetActorLocation();
        ExtractUntil = GetWorld()->GetTimeSeconds() + It->ChannelSeconds();
        if (State->BeginPortalExtraction(**It))
            ShowNotice(It->IsDeep() ? TEXT("正在深层撤离 · 请在金色传送阵内停留 5 秒") :
                TEXT("Extracting: stay inside the portal for 3 seconds"));
        return;
    }
    ShowNotice(TEXT("Press E near the trader or inside the blue extraction portal"));
}

void AARPlayerCharacter::ToggleBranchMap()
{
    if (const auto* PC = Cast<APlayerController>(Controller))
        if (auto* HUD = Cast<AARPrototypeHUD>(PC->GetHUD()))
            if (HUD->GameUI) HUD->GameUI->ToggleBranchMap();
}
float AARPlayerCharacter::ExtractionRemaining() const
{ return FMath::Max(0.f, ExtractUntil - GetWorld()->GetTimeSeconds()); }
void AARPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsDodging()) UpdateDodge(DeltaSeconds);
    else if (bPreviewCharacter) UpdatePreviewAction();
    if (GetWorld()->GetTimeSeconds() > NoticeUntil) Notice.Empty();
    auto* State = Run(this);
    if (CanPlay() && SelectedClass == EARPlayerClass::Mage) Mana = FMath::Min(100.f, Mana + DeltaSeconds * 12.f);
    if (bCampOpen && !IsAtCamp()) bCampOpen = false;
    if (bShopOpen && (!IsValid(ActiveShop) || !ActiveShop->CanTrade(this))) { bShopOpen = false; ActiveShop = nullptr; }
    if (State && State->Phase == EARRunPhase::Extracting)
    {
        if (Health->bDead || FVector::Dist2D(GetActorLocation(), ExtractLocation) > 250)
        { State->CancelExtraction(); ExtractUntil = 0; ShowNotice(TEXT("Extraction cancelled")); }
        else if (ExtractionRemaining() <= 0) State->SettleRun(true, Inventory);
    }
    if (State && GetActorLocation().Z < -500)
    {
        if (CanPlay())
        {
            Health->SetDamageImmunity(0.f);
            Health->ReceiveDamage(Health->MaxHealth);
        }
        else if (State->Phase == EARRunPhase::Hub)
            if (const auto* Mode = GetWorld()->GetAuthGameMode<AARPrototypeGameMode>())
                if (Mode->IsExpeditionTown()) ResetForHub();
    }
}
void AARPlayerCharacter::Damaged()
{
    DodgeTimeRemaining = 0.f;
    if (bPreviewCharacter && !Health->bDead)
    {
        bPreviewAttack = false;
        PlayPreviewClip(TEXT("Hit_A"), false, .24f);
    }
    bShopOpen = false;
    ActiveShop = nullptr;
    if (const auto* PC = Cast<APlayerController>(Controller))
        if (auto* HUD = Cast<AARPrototypeHUD>(PC->GetHUD()))
            if (HUD->GameUI) HUD->GameUI->CloseRouteBoard();
    if (auto* State = Run(this)) State->CancelExtraction();
    ExtractUntil = 0;
    ShowNotice(TEXT("Taking damage!"));
}
void AARPlayerCharacter::Died()
{
    DodgeTimeRemaining = 0.f;
    if (bPreviewCharacter)
    {
        bPreviewAttack = false;
        PlayPreviewClip(TEXT("Death_A"), false);
    }
    bShopOpen = false;
    ActiveShop = nullptr;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    if (auto* State = Run(this)) State->SettleRun(false, Inventory);
}
void AARPlayerCharacter::ReturnOrRetry()
{
    auto* State = Run(this);
    if (!State) return;
    if (State->Phase == EARRunPhase::Settling) { State->RetrySave(); return; }
    if (!State->ReturnToHub()) return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<AARPrototypeGameMode>())
    {
        Mode->ClearRunActors();
        if (Mode->ReturnFromExpedition(this)) return;
    }
    for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It)
    {
        It->EnsureCampLayout();
        break;
    }
    ResetForHub();
}
void AARPlayerCharacter::ResetForHub()
{
    Inventory->Clear();
    ResetCombatForRun();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->StopMovementImmediately();
    const auto* Mode = GetWorld()->GetAuthGameMode<AARPrototypeGameMode>();
    const float Yaw = Mode ? Mode->CampYaw() : 0;
    SetActorLocationAndRotation(Mode ? Mode->CampLocation() : FVector(-700, 0, 100), FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(FRotator(-15, Yaw, 0));
    NextAttackTime = ExtractUntil = 0;
    bInventoryVisible = false;
    bCampOpen = false;
}

AAREnemy::AAREnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42, 96);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SetupMannequin(GetMesh());
    Health = CreateDefaultSubobject<UARHealthComponent>(TEXT("Health"));
    Health->MaxHealth = 60;
    Feedback = CreateDefaultSubobject<UARCombatFeedbackComponent>(TEXT("CombatFeedback"));
    GetCharacterMovement()->MaxWalkSpeed = 180;
    // Greybox pursuit uses character collision, without requiring a baked NavMesh.
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
}
void AAREnemy::BeginPlay()
{
    Super::BeginPlay();
    Health->OnDied.AddDynamic(this, &AAREnemy::Died);
    if (auto* Mode = Cast<AARPrototypeGameMode>(GetWorld()->GetAuthGameMode())) bNavigationEnabled = Mode->bUseNavigation;
}
bool AAREnemy::ClearSight(AActor* Target) const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AREnemySight), false, this);
    return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Target;
}
void AAREnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* State = Run(this);
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || !Player || Health->bDead || !Player->CanPlay())
    { bHasTarget = bWindingAttack = false; GetCharacterMovement()->StopMovementImmediately(); return; }
    const float Time = GetWorld()->GetTimeSeconds();
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Offset.Size();
    if (bWindingAttack)
    {
        if (Time >= HitAt)
        {
            bWindingAttack = false;
            if (Distance <= MeleeReach && FVector::DotProduct(Offset.GetSafeNormal2D(), GetActorForwardVector()) >= AttackFacingDot && ClearSight(Player))
                UGameplayStatics::ApplyDamage(Player, Damage, GetController(), this, nullptr);
        }
        return;
    }
    if (!CanPursue(Player)) return;
    SetActorRotation(Offset.Rotation());
    SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0));
    if (Distance > StopRange || !ClearSight(Player)) Pursue(Player);
    else
    {
        GetCharacterMovement()->StopMovementImmediately();
        if (Time >= NextAttackAt)
        {
            bWindingAttack = true; HitAt = Time + FMath::Max(.1f, AttackWindup); NextAttackAt = Time + FMath::Max(.5f, AttackInterval);
            auto Action = AttackWindup > .6f ? EARCombatAction::Heavy : EARCombatAction::Slash;
            if (Action == EARCombatAction::Slash)
            {
                if (bNextRaiderChop) Action = EARCombatAction::RaiderChop;
                bNextRaiderChop = !bNextRaiderChop;
            }
            Feedback->PlayAction(Action, AttackWindup);
            if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
        }
    }
}
void AAREnemy::Died()
{
    bHasTarget = bWindingAttack = false;
    SetActorEnableCollision(false);
    GetCharacterMovement()->StopMovementImmediately();
    auto* Loot = GetWorld()->SpawnActor<AARLoot>(AARLoot::StaticClass(), GetActorLocation() - FVector(0, 0, 40), FRotator::ZeroRotator);
    if (Loot) { Loot->ItemID = TEXT("Gold"); Loot->Quantity = GoldReward; }
    if (RuneReward > 0)
    {
        const FTransform Transform(GetActorLocation() + FVector(-65, 0, -40));
        auto* Rune = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), Transform);
        if (Rune) { Rune->ItemID = TEXT("Rune"); Rune->Quantity = RuneReward; UGameplayStatics::FinishSpawningActor(Rune, Transform); }
    }
    if (!DropItemID.IsNone())
    {
        const FTransform Transform(GetActorLocation() + FVector(65, 0, -40));
        auto* Gear = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), Transform);
        if (Gear) { Gear->ItemID = DropItemID; Gear->Quantity = 1; UGameplayStatics::FinishSpawningActor(Gear, Transform); }
    }
    const float FallDuration = Feedback->PlayEnemyFall();
    if (FallDuration > 0.f) SetLifeSpan(FallDuration);
    else Destroy();
}

AARLoot::AARLoot()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    // Sphere is available in every UE4 installation; no external art dependency.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(.28f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AARLoot::BeginPlay()
{
    Super::BeginPlay();
    const TCHAR* Path = ItemID == TEXT("Gold") ? TEXT("/Game/AshenRift/Materials/M_Gold.M_Gold") : ItemID == TEXT("Rune") ? TEXT("/Game/AshenRift/Materials/M_Rune.M_Rune") : TEXT("/Game/AshenRift/Materials/M_Potion.M_Potion");
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, Path)) Mesh->SetMaterial(0, Material);
    if (UAREquipmentComponent::IsEquipment(ItemID))
    {
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetRelativeScale3D(FVector(.4f));
        if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Rune.M_Rune"))) Mesh->SetMaterial(0, Mat);
    }
}
bool AARLoot::TryCollect(AARPlayerCharacter* Player)
{
    if (IsActorBeingDestroyed() || !Player || !Player->CanPlay()) return false;
    auto* State = Run(this);
    if (!State || State->Phase != EARRunPhase::InDungeon) return false;
    const bool bAdded = ItemID == TEXT("Gold") ? State->AddCurrency(Quantity, 0) : ItemID == TEXT("Rune") ? State->AddCurrency(0, Quantity) : Player->Inventory->TryAdd(ItemID, Quantity);
    if (!bAdded) return false;
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Pickup, Player->GetActorLocation());
    Player->ShowNotice(FString::Printf(TEXT("Collected %d %s"), Quantity, *ItemID.ToString()));
    SetActorTickEnabled(false);
    Destroy();
    return true;
}
void AARLoot::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AddActorLocalRotation(FRotator(0, 90 * DeltaSeconds, 0));
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 120) TryCollect(Player);
}
AARPortal::AARPortal()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Mesh->SetStaticMesh(Shape.Object);
    Mesh->SetRelativeScale3D(FVector(5, 5, .08f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AARPortal::ConfigureDeep(UARHealthComponent* EliteHealth, int32 Tier)
{
    check(IsValid(EliteHealth));
    RunicBonus = ARExpedition::DeepExtractionBonusRunes(Tier);
    bSealed = !EliteHealth->bDead;
    if (bSealed) EliteHealth->OnDied.AddUniqueDynamic(this, &AARPortal::Unseal);
}

void AARPortal::Unseal()
{
    bSealed = false;
}

bool AARPortal::IsNearby(const AARPlayerCharacter* Player) const
{
    return FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(250.f);
}

FString AARPortal::Prompt() const
{
    if (IsSealed()) return TEXT("金色传送阵已封印 · 击败精英后可用");
    if (IsDeep()) return FString::Printf(TEXT("[E] 深层撤离 · 停留 5 秒 · 成功额外获得 %d 枚符文"), BonusRunes());
    return TEXT("[E] 撤离 · 在蓝色传送阵内停留 3 秒");
}

AARPrototypeGameMode::AARPrototypeGameMode()
{
    DefaultPawnClass = AARPlayerCharacter::StaticClass();
    HUDClass = AARPrototypeHUD::StaticClass();
    EnemyClass = AAREnemy::StaticClass();
    RangedEnemyClass = AARRangedEnemy::StaticClass();
    EliteEnemyClass = AAREliteEnemy::StaticClass();
}
void AARPrototypeGameMode::ClearRunActors()
{
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARLoot> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARSpell> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARBlastZone> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AAREnemyBolt> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARImpactEffect> It(GetWorld()); It; ++It) It->Destroy();
}
bool AARPrototypeGameMode::StartDungeon(AARPlayerCharacter* Player)
{
    if (IsExpeditionTown()) return DepartTown(Player, INDEX_NONE);
    for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It)
        if (It->bUseFixedSeed) return EnterDungeon(Player, It->FixedSeed);
    return EnterDungeon(Player, FMath::Rand());
}

AAREnemy* AARPrototypeGameMode::SpawnExpeditionEnemy(TSubclassOf<AAREnemy> Class, FVector Position, FVector Facing, FName Drop, int32 Tier)
{
    if (!Class) return nullptr;
    const FTransform Transform(FRotator(0, (Facing - Position).Rotation().Yaw, 0), Position);
    auto* Enemy = GetWorld()->SpawnActorDeferred<AAREnemy>(Class, Transform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Enemy) return nullptr;
    Enemy->DropItemID = Drop;
    UGameplayStatics::FinishSpawningActor(Enemy, Transform);
    if (Tier > 1)
    {
        const bool bRuins = ExpeditionSite && IsValid(ExpeditionSite->StandaloneWing);
        const int32 DamagePercent = bRuins ? ARExpedition::RuinsDamageBonusPercent(Tier) : 10 * (Tier - 1);
        const int32 HealthPercent = bRuins ? ARExpedition::RuinsHealthBonusPercent(Tier) : 20 * (Tier - 1);
        Enemy->Damage *= 1.f + DamagePercent / 100.f;
        Enemy->Health->MaxHealth *= 1.f + HealthPercent / 100.f;
        Enemy->Health->ResetHealth();
        Enemy->GoldReward = FMath::RoundToInt(Enemy->GoldReward * ARExpedition::GoldPercent(Tier) / 100.f);
    }
    return Enemy;
}

AAREnemy* AARPrototypeGameMode::SpawnRouteElite(AARPlayerCharacter* Player, FVector Position, FName RewardItem)
{
    const auto* State = Run(this);
    if (!State || State->Phase != EARRunPhase::InDungeon || !IsValid(Player)) return nullptr;
    return SpawnExpeditionEnemy(EliteEnemyClass, Position, Player->GetActorLocation(), RewardItem, State->ExpeditionTier);
}

bool AARPrototypeGameMode::EnterDungeon(AARPlayerCharacter* Player, int32 Seed, int32 Tier)
{
    auto* State = Run(this);
    AARDungeonDirector* Director = nullptr;
    for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
    if (!Player || !State || !Player->IsAtCamp()) return false;
    auto* GeneratedWing = ExpeditionSite && IsValid(ExpeditionSite->StandaloneWing) ? ExpeditionSite->StandaloneWing : nullptr;
    if (GeneratedWing)
    {
        // An incomplete generated world can be discarded before the stash transaction.
        if (!GeneratedWing->bStandalone) return false;
        GeneratedWing->Prepare(Seed, Player->SelectedClass);
        if (!GeneratedWing->Build(Player, Tier)) return false;
    }
    if (!State->BeginExpedition(Seed, Player->Inventory, Tier)) return false;
    if (!GeneratedWing) ClearRunActors();
    if (Director) Director->Generate(Seed);
    Player->ResetCombatForRun();
    Player->bCampOpen = Player->bInventoryVisible = false;
    const FName Weapon = Player->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("OakStaff")) : FName(TEXT("IronSword"));
    for (FName Item : { Weapon, FName(TEXT("LeatherArmor")), FName(TEXT("RunicCharm")) })
        if (Player->Inventory->Count(Item) > 0) Player->Equipment->ToggleEquip(Item);
    // A prepared armor starts the run at full health; toggling armor during combat never heals.
    Player->Health->ResetHealth();
    for (TActorIterator<AARShop> It(GetWorld()); It; ++It) It->ResetStock();
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->GetCharacterMovement()->StopMovementImmediately();
    const FVector Entry = GeneratedWing ? GeneratedWing->StartingLocation() :
        ExpeditionSite ? ExpeditionSite->GetActorLocation() : FVector(1450, 0, 100);
    const float EntryYaw = GeneratedWing ? GeneratedWing->StartingYaw() :
        ExpeditionSite ? ExpeditionSite->GetActorRotation().Yaw : 0;
    Player->SetActorLocationAndRotation(Entry, FRotator(0, EntryYaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
    if (Player->Controller) Player->Controller->SetControlRotation(FRotator(-15, EntryYaw, 0));
    const FVector Supply = GeneratedWing ? Entry + FVector(0, 155, -55) :
        ExpeditionSite ? ExpeditionSite->GetActorTransform().TransformPosition(ExpeditionSite->SupplyOffset) : FVector(1650, 0, 55);
    auto* Potion = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), FTransform(Supply));
    if (Potion)
    {
        Potion->ItemID = TEXT("Potion"); Potion->Quantity = 2;
        UGameplayStatics::FinishSpawningActor(Potion, FTransform(Supply));
    }
    if (GeneratedWing)
    {
        Player->ShowNotice(TEXT("随机遗迹 · 按 M 查看路线和两处撤离点。"));
        return true;
    }
    if (ExpeditionSite)
    {
        const FTransform SiteTransform = ExpeditionSite->GetActorTransform();
        const auto Encounters = ARExpedition::MakeEncounters(ExpeditionSite->EncounterOffsets.Num(), Seed, Tier);
        FRandomStream Rewards(Seed ^ 0x7cf104);
        const FName ClassWeapon = Player->SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
        const FName FixedDrops[] = { TEXT("LeatherArmor"), TEXT("RunicCharm"), ClassWeapon };
        for (int32 Index = 0; Index < ExpeditionSite->EncounterOffsets.Num(); ++Index)
        {
            const FVector Position = SiteTransform.TransformPosition(ExpeditionSite->EncounterOffsets[Index]);
            TSubclassOf<AAREnemy> SpawnClass = EnemyClass;
            if (Encounters[Index] == EARExpeditionEnemy::Ranged) SpawnClass = RangedEnemyClass;
            if (Encounters[Index] == EARExpeditionEnemy::Elite) SpawnClass = EliteEnemyClass;
            FName Drop;
            if (Tier == 0)
            {
                Drop = FixedDrops[FMath::Min(Index, static_cast<int32>(UE_ARRAY_COUNT(FixedDrops)) - 1)];
            }
            else if (Rewards.RandRange(1, 100) <= ARExpedition::EquipmentChance(Tier))
                Drop = ARExpedition::ChooseEquipment(Rewards, Player->SelectedClass);
            SpawnExpeditionEnemy(SpawnClass ? SpawnClass : EnemyClass, Position, Entry, Drop, Tier);
        }
        if (BossClass)
        {
            const FVector Position = SiteTransform.TransformPosition(ExpeditionSite->BossOffset);
            SpawnExpeditionEnemy(BossClass.Get(), Position, Entry, BossClass.GetDefaultObject()->DropItemID, Tier);
        }
        for (TActorIterator<AARExpeditionRouteBoard> It(GetWorld()); It; ++It)
            It->InitializeRun(Seed, Player->SelectedClass);
    }
    else if (Director) Director->Populate(Player, EnemyClass, RangedEnemyClass, EliteEnemyClass, BossClass);
    else
    {
        for (const FVector& Position : { FVector(2400, 0, 100), FVector(3100, 250, 100), FVector(4200, 0, 100) })
            GetWorld()->SpawnActor<AAREnemy>(EnemyClass, Position, FRotator(0, 180, 0));
        if (BossClass) GetWorld()->SpawnActor<AARBoss>(BossClass, FVector(7400, 0, 150), FRotator(0, 180, 0));
    }
    Player->ShowNotice(TEXT("Dungeon started. Defeat enemies or reach the blue extraction portal."));
    return true;
}

