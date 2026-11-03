#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "../ARCombatFeedback.h"
#include "../ARPrototype.h"
#include "../ARProgression.h"
#include "../ARHealthComponent.h"
#include "../ARInventoryComponent.h"
#include "../ARRunGameInstance.h"
#include "../ARGameWidget.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARCombatFeedbackTest, "AshenRift.Integration.CombatFeedback",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
class FARCombatFeedbackCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Stage = 0;
    float WaitUntil = 0, PausedPoseAt = 0;
    double RealUntil = 0;
    FVector RestHand, RestCamera;
    TWeakObjectPtr<AAREnemy> Dummy;
    TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
    TWeakObjectPtr<UAnimMontage> PausedMontage;
    FString CaptureName;
    int32 Frames = 0;
    bool bRequested = false;
    void Capture(const TCHAR* Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("ARCapture")))
        {
#if WITH_EDITOR
            GShaderCompilingManager->FinishAllCompilation();
#endif
            CaptureName = Name; Frames = 0; bRequested = false;
        }
    }
public:
    explicit FARCombatFeedbackCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
        auto* P = World ? Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
        auto* S = World ? Cast<UARRunGameInstance>(World->GetGameInstance()) : nullptr;
        auto* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
        auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
        auto* UI = HUD ? HUD->GameUI : nullptr;
        auto* Audio = UARFeedbackSubsystem::Get(World);
        if (!P || !S || !UI || !Audio || !S->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        { Test->AddError(TEXT("Use L_RandomDungeon -game -ARTestProfile.")); return true; }
        auto* Mode = Cast<AARPrototypeGameMode>(World->GetAuthGameMode());
        if (!CaptureName.IsEmpty())
        {
            if (++Frames < 3) return false;
            if (!bRequested) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), CaptureName), true, false); bRequested = true; return false; }
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            CaptureName.Empty(); return false;
        }
        const float Now = World->GetTimeSeconds();
        if (Now < WaitUntil) return false;
        switch (Stage)
        {
        case 0:
            for (int32 Index = 0; Index <= int32(EARFeedbackCue::Charge); ++Index)
            {
                auto* Sound = Audio->CueAsset(EARFeedbackCue(Index));
                Test->TestTrue(TEXT("Every cue resolves to a short imported sound"), Sound && Sound->GetDuration() > .04f && Sound->GetDuration() < 1);
            }
            for (int32 Index = 0; Index <= int32(EARCombatAction::Whirlwind); ++Index)
            {
                auto* Animation = Audio->Animation(EARCombatAction(Index));
                Test->TestTrue(TEXT("Combat clips are real additive skeletal sequences"), Animation && Animation->IsValidAdditive() && Animation->GetSkeleton() && Animation->GetRawNumberOfFrames() == 25);
            }
            UI->OpenMainMenu();
            Test->TestTrue(TEXT("Sound button is present in main menu"), UI->FindAction(TEXT("Sound")) != nullptr);
            UI->ExecuteAction(TEXT("Sound"));
            Test->TestFalse(TEXT("Sound button disables effects"), Audio->bSoundEnabled);
            Test->TestFalse(TEXT("Muted audio refuses playback"), Audio->PlayCue(EARFeedbackCue::Hit, FVector::ZeroVector));
            UI->ExecuteAction(TEXT("Sound"));
            Test->TestTrue(TEXT("Sound can be re-enabled"), Audio->bSoundEnabled);
            UI->ExecuteAction(TEXT("Start")); UI->ExecuteAction(TEXT("Deploy")); Mode->ClearRunActors();
            P->SetActorLocation(FVector(7000, 0, 100));
            Test->TestTrue(TEXT("Player uses separate slot-enabled animation blueprint"), P->GetMesh()->GetAnimInstance() && P->GetMesh()->GetAnimInstance()->GetClass()->GetName() == TEXT("ABP_ARCombat_C"));
            WaitUntil = Now + .1f; Stage = 1; return false;
        case 1:
            RestHand = P->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component).GetLocation();
            Dummy = World->SpawnActor<AAREnemy>(AAREnemy::StaticClass(), P->GetActorLocation() + FVector(150, 0, 0), FRotator(0, 180, 0));
            if (!Dummy.IsValid()) { Test->AddError(TEXT("Feedback target failed to spawn.")); return true; }
            Dummy->SetActorTickEnabled(false); OriginalMaterial = Dummy->GetMesh()->GetMaterial(0);
            P->Attack(); P->Attack();
            Test->TestEqual(TEXT("Feedback preserves single-hit damage and cooldown"), Dummy->Health->CurrentHealth, 35.f);
            Test->TestTrue(TEXT("Attack plays actual dynamic montage"), P->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying() && P->Feedback->LastAction == EARCombatAction::Slash);
            Test->TestTrue(TEXT("Target flashes on actual damage"), Dummy->Feedback->bFlashing && Dummy->GetMesh()->GetMaterial(0) != OriginalMaterial.Get());
            {
                int32 Numbers = 0;
                for (TActorIterator<AARImpactEffect> It(World); It; ++It) if (It->DamageNumber == 25) ++Numbers;
                Test->TestEqual(TEXT("One floating number records actual lost health"), Numbers, 1);
            }
            Capture(TEXT("M7_HitFlash.png"));
            WaitUntil = Now + .18f; Stage = 2; return false;
        case 2:
            Test->TestTrue(TEXT("Attack changes hand bone pose in the running anim graph"), FVector::Dist(RestHand, P->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component).GetLocation()) > 3);
            Capture(TEXT("M7_SlashHit.png"));
            WaitUntil = Now + .2f; Stage = 3; return false;
        case 3:
            Test->TestFalse(TEXT("Hit flash expires"), Dummy->Feedback->bFlashing);
            Test->TestTrue(TEXT("Hit flash restores exact original material"), Dummy->GetMesh()->GetMaterial(0) == OriginalMaterial.Get());
            RestCamera = P->FollowCamera->GetRelativeLocation();
            P->Health->ReceiveDamage(10);
            P->Feedback->PlayAction(EARCombatAction::Heavy, .9f);
            WaitUntil = Now + .035f; Stage = 4; return false;
        case 4:
            Test->TestTrue(TEXT("Player hit applies a small camera impulse"), FVector::Dist(RestCamera, P->FollowCamera->GetRelativeLocation()) > .01f);
            PausedMontage = P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
            if (!PausedMontage.IsValid()) { Test->AddError(TEXT("Pause test montage is not playing.")); return true; }
            PausedPoseAt = P->GetMesh()->GetAnimInstance()->Montage_GetPosition(PausedMontage.Get());
            UI->TogglePause();
            RealUntil = FPlatformTime::Seconds() + .22; Stage = 5; return false;
        case 5:
            if (FPlatformTime::Seconds() < RealUntil) return false;
            Test->TestTrue(TEXT("Pause freezes montage progression"), FMath::IsNearlyEqual(P->GetMesh()->GetAnimInstance()->Montage_GetPosition(PausedMontage.Get()), PausedPoseAt));
            Test->TestTrue(TEXT("Pause freezes timed hit flash"), P->Feedback->bFlashing);
            UI->ExecuteAction(TEXT("Resume"));
            WaitUntil = Now + .25f; Stage = 6; return false;
        case 6:
            Test->TestFalse(TEXT("Flash resumes and expires after unpause"), P->Feedback->bFlashing);
            Test->TestTrue(TEXT("Camera returns exactly to its rest offset"), P->FollowCamera->GetRelativeLocation().Equals(RestCamera, .001f));
            S->BeginExtraction(); S->SettleRun(true, P->Inventory); UI->Refresh(); UI->ExecuteAction(TEXT("Return"));
            P->SelectClass(EARPlayerClass::Mage); P->Interact(); Mode->ClearRunActors(); UI->Refresh();
            P->SetActorLocation(FVector(7000, 0, 100));
            P->Attack(); P->UseSkill();
            Test->TestTrue(TEXT("Both mage attacks trigger cast pose"), P->Feedback->LastAction == EARCombatAction::Cast && P->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying());
            Test->TestEqual(TEXT("Feedback does not alter spell costs"), P->Mana, 62.f);
            WaitUntil = Now + .18f; Stage = 61; return false;
        case 61:
        {
            int32 Projectiles = 0;
            for (TActorIterator<AARSpell> It(World); It; ++It) if (It->GetOwner() == P) ++Projectiles;
            Test->TestEqual(TEXT("Simultaneous basic and burst do not collide with each other"), Projectiles, 2);
            Capture(TEXT("M7_MageCast.png")); Stage = 7; return false;
        }
        case 7:
            if (FParse::Param(FCommandLine::Get(), TEXT("ARAudioCapture")))
            {
                Mode->ClearRunActors(); Audio->SetSoundEnabled(false); Audio->SetSoundEnabled(true);
                UAudioMixerBlueprintLibrary::StartRecordingOutput(World, 1.f);
                Test->TestTrue(TEXT("Actual audio voice starts for mixer recording"), Audio->PlayCue(EARFeedbackCue::Extract, P->GetActorLocation(), true));
                RealUntil = FPlatformTime::Seconds() + 1;
                Stage = 8; return false;
            }
            Stage = 10; return false;
        case 8:
            if (FPlatformTime::Seconds() < RealUntil) return false;
            UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, TEXT("M7_CombatMix"), FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AudioCaptures")));
            RealUntil = FPlatformTime::Seconds() + 5; Stage = 9; return false;
        case 9:
        {
            const FString File = FPaths::ProjectSavedDir() / TEXT("AudioCaptures/M7_CombatMix.wav");
            if (IFileManager::Get().FileSize(*File) <= 44 && FPlatformTime::Seconds() < RealUntil) return false;
            Test->TestTrue(TEXT("Audio mixer exported a nonempty PCM recording"), IFileManager::Get().FileSize(*File) > 44);
            Stage = 10; return false;
        }
        case 10:
            P->Health->ReceiveDamage(1000); UI->Refresh(); UI->ExecuteAction(TEXT("Return"));
            Test->TestFalse(TEXT("Respawn clears feedback flashes"), P->Feedback->bFlashing);
            Test->TestFalse(TEXT("Run cleanup clears temporary impact effects"), bool(TActorIterator<AARImpactEffect>(World)));
            WaitUntil = Now + .05f; Stage = 11; return false;
        case 11:
            Test->TestFalse(TEXT("Respawn clears attack montages on animation update"), P->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying());
            UGameplayStatics::DeleteGameInSlot(S->GetProfileSlot(), 0); return true;
        }
        return false;
    }
};
bool FARCombatFeedbackTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ARTestProfile"))) { AddError(TEXT("Use -ARTestProfile.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FARCombatFeedbackCommand(this)); return true;
}
#endif
