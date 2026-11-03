#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "ARCombatFeedback.generated.h"

class USoundBase;
class USoundAttenuation;
class UAnimSequence;
class UAnimMontage;
class UAudioComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextRenderComponent;
class UCameraComponent;

UENUM(BlueprintType)
enum class EARFeedbackCue : uint8 { Swing, Hit, Cast, Blast, Hurt, Heal, Pickup, Extract, Death, Click, Charge };
UENUM(BlueprintType)
enum class EARCombatAction : uint8 { Slash, Cast, Heavy, Hurt, Whirlwind };

UCLASS()
class ASHENRIFT_API UARFeedbackSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UPROPERTY(BlueprintReadOnly) bool bSoundEnabled = true;
    UFUNCTION(BlueprintCallable) void SetSoundEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable) bool PlayCue(EARFeedbackCue Cue, FVector Location, bool bUI = false);
    USoundBase* CueAsset(EARFeedbackCue Cue) const { return Sounds.FindRef(Cue); }
    UAnimSequence* Animation(EARCombatAction Action) const { return Animations.FindRef(Action); }
    static UARFeedbackSubsystem* Get(const UObject* Context);
private:
    UPROPERTY() TMap<EARFeedbackCue, USoundBase*> Sounds;
    UPROPERTY() TMap<EARCombatAction, UAnimSequence*> Animations;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    TArray<TWeakObjectPtr<UAudioComponent>> Voices;
    TMap<EARFeedbackCue, double> LastCueAt;
};

UCLASS(ClassGroup=(AshenRift), meta=(BlueprintSpawnableComponent))
class ASHENRIFT_API UARCombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARCombatFeedbackComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCameraFeedback = true;
    UPROPERTY(BlueprintReadOnly) bool bFlashing = false;
    UPROPERTY(BlueprintReadOnly) EARCombatAction LastAction = EARCombatAction::Slash;
    UFUNCTION(BlueprintCallable) bool PlayAction(EARCombatAction Action, float Duration = 0);
    UFUNCTION(BlueprintCallable) void ResetFeedback();
private:
    UPROPERTY() USkeletalMeshComponent* Body = nullptr;
    UPROPERTY() UMaterialInterface* FlashMaterial = nullptr;
    UPROPERTY() TArray<UMaterialInterface*> RestoreMaterials;
    UPROPERTY() UCameraComponent* Camera = nullptr;
    float FlashUntil = 0, ShakeUntil = 0, PreviousHealth = 0, LastDamage = 0;
    FVector CameraRest;
    UFUNCTION() void HealthChanged(float Current, float Maximum);
    UFUNCTION() void Damaged();
    UFUNCTION() void Died();
    void RestoreFlash();
};

UCLASS()
class ASHENRIFT_API AARImpactEffect : public AActor
{
    GENERATED_BODY()
public:
    AARImpactEffect();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageNumber = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor(1, .65f, .15f);
    UPROPERTY(VisibleAnywhere) UTextRenderComponent* Number;
    static AARImpactEffect* Spawn(const UObject* Context, FVector Location, float Damage, FLinearColor InColor);
private:
    UPROPERTY() TArray<UStaticMeshComponent*> Shards;
    float Age = 0;
};
