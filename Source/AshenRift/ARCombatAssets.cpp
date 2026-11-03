#include "ARCombatAssets.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

namespace
{
    bool SaveCombatAsset(UObject* Asset)
    {
        Asset->MarkPackageDirty();
        return UPackage::SavePackage(Asset->GetOutermost(), Asset, RF_Public | RF_Standalone,
            *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()));
    }
    bool CreateSequence(USkeleton* Skeleton, const TCHAR* Name, int32 Kind, float Length)
    {
        const FString PackageName = FString(TEXT("/Game/AshenRift/Animations/")) + Name;
        if (FPackageName::DoesPackageExist(PackageName)) return true;
        auto* Animation = NewObject<UAnimSequence>(CreatePackage(*PackageName), Name, RF_Public | RF_Standalone);
        Animation->SetSkeleton(Skeleton);
        Animation->AdditiveAnimType = AAT_LocalSpaceBase;
        Animation->RefPoseType = ABPT_RefPose;
        Animation->SequenceLength = Length;
        const int32 Frames = 25;
        Animation->SetRawNumberOfFrame(Frames);
        const auto& Ref = Skeleton->GetReferenceSkeleton();
        const auto& Local = Ref.GetRefBonePose();
        TArray<FTransform> Component; Component.SetNum(Local.Num());
        for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
            Component[Bone] = Ref.GetParentIndex(Bone) < 0 ? Local[Bone] : Local[Bone] * Component[Ref.GetParentIndex(Bone)];
        for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
        {
            FRawAnimSequenceTrack Track;
            const FName BoneName = Ref.GetBoneName(Bone);
            const int32 Parent = Ref.GetParentIndex(Bone);
            const FQuat ParentRotation = Parent < 0 ? FQuat::Identity : Component[Parent].GetRotation();
            for (int32 Frame = 0; Frame < Frames; ++Frame)
            {
                const float T = float(Frame) / (Frames - 1);
                const float Pulse = FMath::Sin(PI * T);
                const float Sweep = T < .28f ? FMath::Lerp(0.f, -35.f, T / .28f) : T < .7f ? FMath::Lerp(-35.f, 70.f, (T - .28f) / .42f) : FMath::Lerp(70.f, 0.f, (T - .7f) / .3f);
                float Pitch = 0, Yaw = 0;
                if (Kind == 3)
                {
                    if (BoneName == TEXT("spine_01")) Pitch = -13 * Pulse;
                    if (BoneName == TEXT("head")) Pitch = 9 * Pulse;
                }
                else
                {
                    if (BoneName == TEXT("spine_01")) { Pitch = Kind == 2 ? -12 * Pulse : 0; Yaw = Kind == 1 ? 0 : Sweep * .25f; }
                    if (BoneName == TEXT("upperarm_r")) { Pitch = (Kind == 2 ? 100.f : Kind == 1 ? 65.f : 55.f) * Pulse; Yaw = Kind == 1 ? 5 * Pulse : Sweep; }
                    if (BoneName == TEXT("lowerarm_r")) { Pitch = 28 * Pulse; Yaw = Kind == 1 ? 0 : -25 * Pulse; }
                    if (BoneName == TEXT("upperarm_l")) { Pitch = (Kind == 2 ? 100.f : Kind == 1 ? 48.f : Kind == 4 ? 55.f : 20.f) * Pulse; Yaw = Kind == 4 ? -Sweep : 0; }
                }
                const FQuat Desired = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Pitch));
                const FQuat Delta = ParentRotation.Inverse() * Desired * ParentRotation;
                Track.PosKeys.Add(Local[Bone].GetTranslation());
                Track.RotKeys.Add((Delta * Local[Bone].GetRotation()).GetNormalized());
                Track.ScaleKeys.Add(Local[Bone].GetScale3D());
            }
            Animation->AddNewRawTrack(BoneName, &Track);
        }
        Animation->PostProcessSequence();
        FAssetRegistryModule::AssetCreated(Animation);
        return SaveCombatAsset(Animation);
    }
}
#endif
bool UARCombatAssets::CreateCombatAnimations()
{
#if WITH_EDITOR
    auto* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton.UE4_Mannequin_Skeleton"));
    if (!Skeleton) return false;
    const TCHAR* Names[] = { TEXT("A_AR_Slash"), TEXT("A_AR_Cast"), TEXT("A_AR_Heavy"), TEXT("A_AR_Hurt"), TEXT("A_AR_Whirlwind") };
    const float Lengths[] = { .42f, .42f, .9f, .22f, .55f };
    for (int32 Kind = 0; Kind < 5; ++Kind) if (!CreateSequence(Skeleton, Names[Kind], Kind, Lengths[Kind])) return false;
    const FString PackageName = TEXT("/Game/AshenRift/Animations/ABP_ARCombat");
    if (FPackageName::DoesPackageExist(PackageName)) return true;
    auto* Original = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP"));
    if (!Original) return false;
    auto* Blueprint = Cast<UAnimBlueprint>(FAssetToolsModule::GetModule().Get().DuplicateAsset(TEXT("ABP_ARCombat"), TEXT("/Game/AshenRift/Animations"), Original));
    if (!Blueprint) return false;
    Blueprint->SetFlags(RF_Public | RF_Standalone);
    bool bLinked = false;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Root = Cast<UAnimGraphNode_Root>(Node))
            {
                UEdGraphPin* Input = nullptr;
                for (UEdGraphPin* Pin : Root->Pins) if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 1) { Input = Pin; break; }
                if (!Input) continue;
                auto* Source = Input->LinkedTo[0];
                auto* SlotNode = NewObject<UAnimGraphNode_Slot>(Graph);
                SlotNode->Node.SlotName = TEXT("DefaultSlot");
                SlotNode->NodePosX = Root->NodePosX - 240; SlotNode->NodePosY = Root->NodePosY;
                SlotNode->CreateNewGuid(); SlotNode->PostPlacedNewNode(); SlotNode->AllocateDefaultPins();
                UEdGraphPin* SlotIn = nullptr; UEdGraphPin* SlotOut = nullptr;
                for (UEdGraphPin* Pin : SlotNode->Pins) { if (Pin->Direction == EGPD_Input && Pin->PinName == TEXT("Source")) SlotIn = Pin; if (Pin->Direction == EGPD_Output) SlotOut = Pin; }
                if (!SlotIn || !SlotOut) return false;
                Input->BreakAllPinLinks(); Source->MakeLinkTo(SlotIn); SlotOut->MakeLinkTo(Input);
                Graph->AddNode(SlotNode, false, false);
                bLinked = true;
                break;
            }
    if (!bLinked) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return false;
    FAssetRegistryModule::AssetCreated(Blueprint);
    return SaveCombatAsset(Blueprint);
#else
    return false;
#endif
}
