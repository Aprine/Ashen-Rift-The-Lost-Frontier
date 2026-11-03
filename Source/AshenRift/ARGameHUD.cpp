#include "ARPrototype.h"
#include "ARGameWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AARPrototypeHUD::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Controller = GetOwningPlayerController())
    {
        GameUI = CreateWidget<UARGameWidget>(Controller, UARGameWidget::StaticClass());
        if (GameUI)
        {
            // Gameplay regressions skip the initial menu but still exercise the real UI.
            GameUI->InitializeMenu(!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile")));
            GameUI->AddToViewport();
        }
    }
}
void AARPrototypeHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GameUI) { GameUI->RemoveFromParent(); GameUI = nullptr; }
    Super::EndPlay(Reason);
}
void AARPrototypeHUD::DrawHUD() { Super::DrawHUD(); }
bool AARPlayerCharacter::IsUIBlockingInput() const
{
    auto* PC = Cast<APlayerController>(Controller);
    auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
    return HUD && HUD->GameUI && HUD->GameUI->BlocksGameplayInput();
}
void AARPlayerCharacter::LookYaw(float Value) { if (!IsUIBlockingInput()) AddControllerYawInput(Value); }
void AARPlayerCharacter::LookPitch(float Value) { if (!IsUIBlockingInput()) AddControllerPitchInput(Value); }
void AARPlayerCharacter::TogglePauseMenu()
{
    auto* PC = Cast<APlayerController>(Controller);
    auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
    if (HUD && HUD->GameUI) HUD->GameUI->TogglePause();
}
