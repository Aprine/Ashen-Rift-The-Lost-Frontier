#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ARGameWidget.generated.h"

class UARGameWidget;
class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UBorder;
class UProgressBar;
class UPanelWidget;
class AARPlayerCharacter;
class AARExpeditionRouteBoard;
class AARExpeditionBranchWing;
class UARRunGameInstance;
enum class EARExpeditionRoute : uint8;

UCLASS()
class ASHENRIFT_API UARActionButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY() UARGameWidget* Screen = nullptr;
    UPROPERTY() FName Action;
    UPROPERTY() int32 Payload = 0;
    UFUNCTION() void InvokeAction();
};

UENUM(BlueprintType)
enum class EARUIScreen : uint8 { Game, MainMenu, Pause, Camp, Inventory, Shop, Result, Routes, BranchMap };

UCLASS(Blueprintable)
class ASHENRIFT_API UARGameWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
    UFUNCTION(BlueprintCallable) bool ExecuteAction(FName Action, int32 Payload = 0);
    UFUNCTION(BlueprintCallable) void TogglePause();
    UFUNCTION(BlueprintCallable) bool OpenMainMenu();
    UFUNCTION(BlueprintPure) EARUIScreen CurrentScreen() const { return View; }
    bool BlocksGameplayInput() const;
    bool OpenRouteBoard(AARExpeditionRouteBoard* Board);
    bool ToggleBranchMap();
    void CloseRouteBoard();
    bool IsMenuOpen() const { return bMainMenu || bPaused; }
    void InitializeMenu(bool bShow) { bMainMenu = bShow; }
    void Refresh();
    FString LabelText(FName Key) const;
    UARActionButton* FindAction(FName Action, int32 Payload = 0) const { return Actions.FindRef(FName(*(Action.ToString() + FString::FromInt(Payload)))); }
    static FString ItemName(FName Item);
    static FString TranslateNotice(const FString& Message);
private:
    UPROPERTY() UCanvasPanel* RootCanvas = nullptr;
    UPROPERTY() UBorder* Modal = nullptr;
    UPROPERTY() UVerticalBox* ModalBody = nullptr;
    UPROPERTY() UBorder* StatusPanel = nullptr;
    UPROPERTY() UBorder* PromptPanel = nullptr;
    UPROPERTY() UBorder* FooterPanel = nullptr;
    UPROPERTY() UBorder* Dimmer = nullptr;
    UPROPERTY() UProgressBar* HealthBar = nullptr;
    UPROPERTY() UProgressBar* ManaBar = nullptr;
    UPROPERTY() UProgressBar* BossBar = nullptr;
    UPROPERTY() UBorder* BossPanel = nullptr;
    UPROPERTY() TMap<FName, UTextBlock*> Labels;
    UPROPERTY() TMap<FName, UARActionButton*> Actions;
    EARUIScreen View = EARUIScreen::Game;
    bool bBuilt = false;
    bool bMainMenu = true;
    bool bPaused = false;
    TWeakObjectPtr<AARExpeditionRouteBoard> OpenBoard;
    TWeakObjectPtr<AARExpeditionBranchWing> OpenBranch;
    EARExpeditionRoute TrackedRoute{};
    int32 TrackedBranchRoom = INDEX_NONE;
    TMap<FName, FString> RouteDistances;
    FString RouteGuidance;
    float NextRouteUpdate = 0.f;
    void BuildRoot();
    void BuildBranchMap();
    void UpdateRouteGuidance(AARPlayerCharacter* P, const UARRunGameInstance* State);
    void BuildScreen(EARUIScreen Screen);
    void ApplyInputMode();
    UTextBlock* Text(UPanelWidget* Parent, const FString& Value, int32 Size = 16, FLinearColor Color = FLinearColor::White, FName Key = NAME_None);
    UARActionButton* Button(UPanelWidget* Parent, const FString& Value, FName Action, int32 Payload = 0, FName LabelKey = NAME_None);
    UBorder* Panel(UCanvasPanel* Parent, FVector2D Position, FVector2D Size, FLinearColor Color);
    void Gap(UPanelWidget* Parent, float Height);
    AARPlayerCharacter* Player() const;
    UARRunGameInstance* Run() const;
};
