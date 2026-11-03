#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "ARCoreTypes.h"
#include "ARPrototype.generated.h"

class UARHealthComponent;
class UARInventoryComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class AARBoss;
class AARShop;
class UAREquipmentComponent;
class UARGameWidget;
class UARCombatFeedbackComponent;

UCLASS()
class ASHENRIFT_API AARPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AARPlayerCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARHealthComponent* Health;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARInventoryComponent* Inventory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UAREquipmentComponent* Equipment;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARCombatFeedbackComponent* Feedback;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USpringArmComponent* CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UCameraComponent* FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Sword;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackDamage = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackCooldown = .6f;
    UPROPERTY(BlueprintReadOnly) bool bInventoryVisible = false;
    UPROPERTY(BlueprintReadOnly) bool bCampOpen = false;
    UPROPERTY(BlueprintReadOnly) int32 CampRow = 0;
    UPROPERTY(BlueprintReadOnly) FString Notice;
    UPROPERTY(BlueprintReadOnly) EARPlayerClass SelectedClass = EARPlayerClass::Warrior;
    UPROPERTY(BlueprintReadOnly) float Mana = 100.f;
    UPROPERTY(BlueprintReadOnly) int32 RunPowerBonus = 0;
    UPROPERTY(BlueprintReadOnly) bool bShopOpen = false;
    UPROPERTY() AARShop* ActiveShop = nullptr;
    UFUNCTION(BlueprintCallable) void Attack();
    UFUNCTION(BlueprintCallable) void Interact();
    UFUNCTION(BlueprintCallable) void UsePotion();
    UFUNCTION(BlueprintCallable) void ReturnOrRetry();
    UFUNCTION(BlueprintCallable) void ResetForHub();
    UFUNCTION(BlueprintCallable) bool SelectClass(EARPlayerClass Class);
    UFUNCTION(BlueprintCallable) void UseSkill();
    UFUNCTION(BlueprintCallable) void ResetCombatForRun();
    float SkillRemaining() const;
    float ExtractionRemaining() const;
    bool CanPlay() const;
    void ShowNotice(const FString& Text);
    int32 TotalDamageBonus() const;
    float BaseMaxHealth() const;
    bool IsAtCamp() const;
    bool IsUIBlockingInput() const;
    UFUNCTION(BlueprintCallable) void ToggleCamp();
    UFUNCTION(BlueprintCallable) void CampPrevious();
    UFUNCTION(BlueprintCallable) void CampNext();
    UFUNCTION(BlueprintCallable) void CampAdd();
    UFUNCTION(BlueprintCallable) void CampRemove();
    UFUNCTION(BlueprintCallable) void BuyVitality();
    UFUNCTION(BlueprintCallable) void BuyMight();
    static FName CampItem(int32 Row);
private:
    float NextAttackTime = 0.f;
    float NoticeUntil = 0.f;
    float ExtractUntil = 0.f;
    float NextSkillTime = 0.f;
    FVector ExtractLocation;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void TogglePauseMenu();
    void SafeJump();
    void ToggleInventory();
    void OptionOne();
    void OptionTwo();
    void FireSpell(bool bHeavy);
    void EquipWeapon();
    void EquipArmor();
    void EquipCharm();
    UFUNCTION() void Died();
    UFUNCTION() void Damaged();
};

UCLASS()
class ASHENRIFT_API AAREnemy : public ACharacter
{
    GENERATED_BODY()
public:
    AAREnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARHealthComponent* Health;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARCombatFeedbackComponent* Feedback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float Damage = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackInterval = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") int32 GoldReward = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") int32 RuneReward = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MeleeReach = 175;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float StopRange = 145;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackWindup = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackFacingDot = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") FString DisplayName = TEXT("RAIDER");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float DetectRange = 1250.f;
    UPROPERTY(BlueprintReadOnly) bool bWindingAttack = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DropItemID;
    UPROPERTY(BlueprintReadOnly) int32 NavigationPointCount = 0;
protected:
    float HitAt = 0.f;
    float NextAttackAt = 0.f;
    UFUNCTION() virtual void Died();
    bool ClearSight(AActor* Target) const;
    bool CanPursue(AActor* Target);
    void Pursue(AActor* Target);
    bool bHasTarget = false;
    bool bNavigationEnabled = false;
    float NextPathAt = 0;
    int32 PathIndex = 0;
    TArray<FVector> PathPoints;
};

UCLASS()
class ASHENRIFT_API AARLoot : public AActor
{
    GENERATED_BODY()
public:
    AARLoot();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID = TEXT("Gold");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 10;
    UFUNCTION(BlueprintCallable) bool TryCollect(AARPlayerCharacter* Player);
};

UCLASS()
class ASHENRIFT_API AARPortal : public AActor
{
    GENERATED_BODY()
public:
    AARPortal();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* Mesh;
};

UCLASS()
class ASHENRIFT_API AARPrototypeGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AARPrototypeGameMode();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AAREnemy> EnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AAREnemy> RangedEnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AAREnemy> EliteEnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AARBoss> BossClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseNavigation = false;
    UFUNCTION(BlueprintCallable) bool StartDungeon(AARPlayerCharacter* Player);
    UFUNCTION(BlueprintCallable) void ClearRunActors();
};

UCLASS()
class ASHENRIFT_API AARPrototypeHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void DrawHUD() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UARGameWidget* GameUI = nullptr;
};
