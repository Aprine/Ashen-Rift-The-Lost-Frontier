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
    struct FEnemyPoseKey
    {
        const TCHAR* Bone;
        FVector Ready;
        FVector Contact;
    };

    void EnemyPose(int32 Kind, FName Bone, float T, FVector& Angles, FVector& Offset)
    {
        // Rotations use mannequin mesh axes: X pitch, Y side tilt, Z turn.
        static const FEnemyPoseKey Raider[] = {
            {TEXT("spine_01"), {8,0,-22}, {-13,0,28}}, {TEXT("upperarm_r"), {20,0,-62}, {58,0,82}},
            {TEXT("lowerarm_r"), {56,0,0}, {13,0,-20}}, {TEXT("upperarm_l"), {23,0,12}, {32,0,-15}}};
        static const FEnemyPoseKey Hexer[] = {
            {TEXT("spine_01"), {9,0,-10}, {-12,0,8}}, {TEXT("upperarm_r"), {89,0,-12}, {54,0,22}},
            {TEXT("lowerarm_r"), {54,0,14}, {8,0,5}}, {TEXT("upperarm_l"), {51,0,21}, {28,0,-8}}};
        static const FEnemyPoseKey Sentinel[] = {
            {TEXT("spine_01"), {3,0,-5}, {-8,0,6}},
            {TEXT("upperarm_r"), {32,0,-8}, {72,0,5}}, {TEXT("lowerarm_r"), {55,0,-9}, {7,0,4}},
            {TEXT("upperarm_l"), {10,0,4}, {12,0,6}}, {TEXT("lowerarm_l"), {14,0,0}, {18,0,0}}};
        static const FEnemyPoseKey Hurt[] = {
            {TEXT("spine_01"), {18,10,-12}, {8,4,-4}}, {TEXT("head"), {-9,4,0}, {-4,0,0}},
            {TEXT("upperarm_r"), {-26,0,-16}, {-11,0,-6}}, {TEXT("upperarm_l"), {-20,0,14}, {-6,0,5}}};
        static const FEnemyPoseKey BossMelee[] = {
            {TEXT("spine_01"), {4,0,-16}, {-10,0,22}}, {TEXT("head"), {0,0,7}, {3,0,-9}},
            {TEXT("upperarm_r"), {18,0,-38}, {54,0,48}}, {TEXT("lowerarm_r"), {38,0,0}, {13,0,-8}},
            {TEXT("upperarm_l"), {7,0,4}, {12,0,-13}}};
        static const FEnemyPoseKey BossArea[] = {
            {TEXT("spine_01"), {9,0,0}, {-27,0,0}}, {TEXT("head"), {-4,0,0}, {12,0,0}},
            {TEXT("upperarm_r"), {71,0,-12}, {23,0,-9}}, {TEXT("upperarm_l"), {71,0,12}, {23,0,9}},
            {TEXT("lowerarm_r"), {33,0,0}, {11,0,0}}, {TEXT("lowerarm_l"), {33,0,0}, {11,0,0}}};
        static const FEnemyPoseKey RaiderChop[] = {
            {TEXT("spine_01"), {12,0,-4}, {-21,0,3}}, {TEXT("head"), {-6,0,0}, {5,0,0}},
            {TEXT("upperarm_r"), {98,0,-11}, {35,0,12}}, {TEXT("lowerarm_r"), {42,0,-9}, {10,0,0}},
            {TEXT("upperarm_l"), {10,0,5}, {26,0,-8}}};
        static const FEnemyPoseKey BossElbow[] = {
            {TEXT("spine_01"), {4,0,15}, {-13,0,-18}}, {TEXT("head"), {0,0,-9}, {5,0,7}},
            {TEXT("upperarm_l"), {17,0,21}, {63,0,-24}}, {TEXT("lowerarm_l"), {66,0,9}, {83,0,-6}},
            {TEXT("upperarm_r"), {13,0,-6}, {9,0,12}}};
        if (Kind == 4)
        {
            const float Stagger = FMath::SmoothStep(0.f, .13f, T) * (1.f - FMath::SmoothStep(.21f, .46f, T));
            const float Buckle = FMath::SmoothStep(.12f, .44f, T) * (1.f - .68f * FMath::SmoothStep(.48f, .86f, T));
            const float Tip = FMath::SmoothStep(.31f, .76f, T);
            const float Settle = FMath::SmoothStep(.76f, .93f, T);
            if (Bone == TEXT("pelvis"))
            {
                Angles = FVector(4.f * Stagger - 11.f * Buckle, -10.f * Stagger - 80.f * Tip - 6.f * Settle, 11.f * Tip);
                Offset = FVector(-24.f * Tip, -9.f * Stagger - 13.f * Tip, -24.f * Buckle - 60.f * Tip - 4.f * Settle);
            }
            if (Bone == TEXT("spine_01")) Angles = FVector(16.f * Stagger - 18.f * Buckle, 13.f * Tip, -8.f * Stagger);
            if (Bone == TEXT("head")) Angles = FVector(-10.f * Stagger + 12.f * Buckle, 8.f * Tip, 5.f * Stagger);
            if (Bone == TEXT("upperarm_r")) Angles = FVector(31.f * Buckle + 9.f * Tip, 4.f * Tip, -22.f * Buckle - 13.f * Tip);
            if (Bone == TEXT("lowerarm_r")) Angles.X = 27.f * Buckle + 8.f * Tip;
            if (Bone == TEXT("upperarm_l")) Angles = FVector(12.f * Buckle - 11.f * Tip, 0, 22.f * Buckle + 19.f * Tip);
            if (Bone == TEXT("lowerarm_l")) Angles.X = 16.f * Buckle + 6.f * Settle;
            if (Bone == TEXT("thigh_r")) Angles.X = 36.f * Buckle;
            if (Bone == TEXT("thigh_l")) Angles.X = 24.f * Buckle - 8.f * Tip;
            if (Bone == TEXT("calf_r")) Angles.X = -61.f * Buckle - 8.f * Tip;
            if (Bone == TEXT("calf_l")) Angles.X = -45.f * Buckle - 5.f * Tip;
            return;
        }
        if (Kind == 9)
        {
            const float Kneel = FMath::SmoothStep(.08f, .47f, T);
            const float Collapse = FMath::SmoothStep(.43f, .88f, T);
            const float Brace = Kneel * (1.f - .72f * Collapse);
            if (Bone == TEXT("pelvis"))
            {
                Angles = FVector(-10.f * Kneel - 65.f * Collapse, -12.f * Collapse, 7.f * Collapse);
                Offset = FVector(-12.f * Collapse, 14.f * Kneel + 35.f * Collapse, -39.f * Kneel - 35.f * Collapse);
            }
            if (Bone == TEXT("spine_01")) Angles = FVector(-14.f * Brace + 10.f * Collapse, 9.f * Collapse, 0.f);
            if (Bone == TEXT("head")) Angles.X = 14.f * FMath::SmoothStep(.58f, .92f, T);
            if (Bone == TEXT("thigh_l")) Angles.X = 40.f * Kneel;
            if (Bone == TEXT("thigh_r")) Angles.X = 28.f * Kneel;
            if (Bone == TEXT("calf_l")) Angles.X = -67.f * Kneel;
            if (Bone == TEXT("calf_r")) Angles.X = -52.f * Kneel;
            if (Bone == TEXT("upperarm_l")) Angles = FVector(53.f * Brace + 18.f * Collapse, 0.f, -9.f * Kneel);
            if (Bone == TEXT("upperarm_r")) Angles = FVector(72.f * Brace + 25.f * Collapse, 0.f, 15.f * Kneel);
            if (Bone == TEXT("lowerarm_l") || Bone == TEXT("lowerarm_r")) Angles.X = 23.f * Brace + 7.f * Collapse;
            return;
        }
        const TArrayView<const FEnemyPoseKey> Keys[] = {MakeArrayView(Raider), MakeArrayView(Hexer), MakeArrayView(Sentinel),
            MakeArrayView(Hurt), {}, MakeArrayView(BossMelee), MakeArrayView(BossArea), MakeArrayView(RaiderChop), MakeArrayView(BossElbow)};
        const float ReadyAt = Kind == 3 ? .18f : .48f;
        const float ContactAt = Kind == 3 ? .4f : .64f;
        for (const FEnemyPoseKey& Key : Keys[Kind])
        {
            if (Bone != Key.Bone) continue;
            if (T < ReadyAt) Angles = Key.Ready * FMath::SmoothStep(0.f, ReadyAt, T);
            else if (T < ContactAt) Angles = FMath::Lerp(Key.Ready, Key.Contact, FMath::SmoothStep(ReadyAt, ContactAt, T));
            else Angles = Key.Contact * (1.f - FMath::SmoothStep(ContactAt, 1.f, T));
            break;
        }
    }

    bool SaveCombatAsset(UObject* Asset)
    {
        Asset->MarkPackageDirty();
        return UPackage::SavePackage(Asset->GetOutermost(), Asset, RF_Public | RF_Standalone,
            *FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()));
    }
    bool CreateSequence(USkeleton* Skeleton, const TCHAR* Name, int32 Kind, float Length, bool bEnemy = false)
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
                FVector Angles = FVector::ZeroVector;
                FVector Offset = FVector::ZeroVector;
                float& Pitch = Angles.X;
                float& Yaw = Angles.Z;
                if (bEnemy) EnemyPose(Kind, BoneName, T, Angles, Offset);
                else if (Kind == 3)
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
                const FQuat Desired = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) *
                    FQuat(FVector::RightVector, FMath::DegreesToRadians(Angles.Y)) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Pitch));
                const FQuat Delta = ParentRotation.Inverse() * Desired * ParentRotation;
                Track.PosKeys.Add(Local[Bone].GetTranslation() + ParentRotation.UnrotateVector(Offset));
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
bool UARCombatAssets::CreateEnemyAnimations()
{
#if WITH_EDITOR
    auto* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton.UE4_Mannequin_Skeleton"));
    if (!Skeleton) return false;
    const TCHAR* Names[] = {TEXT("A_AR_RaiderSwing"), TEXT("A_AR_HexerCast"), TEXT("A_AR_SentinelShieldJab"),
        TEXT("A_AR_EnemyRecoil"), TEXT("A_AR_EnemyStaggerFall"), TEXT("A_AR_WardenSweep"), TEXT("A_AR_WardenGroundSlam"),
        TEXT("A_AR_RaiderChop"), TEXT("A_AR_WardenElbow"), TEXT("A_AR_WardenKneelFall")};
    const float Lengths[] = {.7f, 1.2f, 1.4f, .25f, 1.34f, 1.f, 1.8f, .72f, .98f, 1.85f};
    for (int32 Kind = 0; Kind < UE_ARRAY_COUNT(Names); ++Kind)
        if (!CreateSequence(Skeleton, Names[Kind], Kind, Lengths[Kind], true)) return false;
    return true;
#else
    return false;
#endif
}

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
