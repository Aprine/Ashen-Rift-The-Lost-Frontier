#include "ARPreviewAnimInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/PoseSnapshot.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
    struct FARPreviewAnimProxy : FAnimSingleNodeInstanceProxy
    {
        explicit FARPreviewAnimProxy(UAnimInstance* Owner) : FAnimSingleNodeInstanceProxy(Owner) {}

        FPoseSnapshot PreviousPose;
        float Remaining = 0.f;
        float Duration = 0.f;

        void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
        {
            FAnimSingleNodeInstanceProxy::UpdateAnimationNode(Context);
            Remaining = FMath::Max(0.f, Remaining - Context.GetDeltaTime());
        }

        bool Evaluate(FPoseContext& Output) override
        {
            const bool bEvaluated = FAnimSingleNodeInstanceProxy::Evaluate(Output);
            if (!bEvaluated || Remaining <= 0.f || !PreviousPose.bIsValid) return bEvaluated;

            const float Alpha = FMath::SmoothStep(0.f, 1.f, 1.f - Remaining / Duration);
            const auto& Bones = Output.Pose.GetBoneContainer();
            for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
            {
                const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
                if (PreviousPose.LocalTransforms.IsValidIndex(MeshIndex))
                {
                    FTransform Blended;
                    Blended.Blend(PreviousPose.LocalTransforms[MeshIndex], Output.Pose[Bone], Alpha);
                    Output.Pose[Bone] = Blended;
                }
            }
            return true;
        }
    };
}

FAnimInstanceProxy* UARPreviewAnimInstance::CreateAnimInstanceProxy()
{
    return new FARPreviewAnimProxy(this);
}

void UARPreviewAnimInstance::BlendTo(UAnimationAsset* Asset, bool bLoop, float PlayRate, float BlendSeconds)
{
    auto& Proxy = GetProxyOnGameThread<FARPreviewAnimProxy>();
    Proxy.Remaining = Proxy.Duration = GetAnimationAsset() ? BlendSeconds : 0.f;
    if (Proxy.Remaining > 0.f) GetSkelMeshComponent()->SnapshotPose(Proxy.PreviousPose);
    SetAnimationAsset(Asset, bLoop, PlayRate);
    PlayAnim(bLoop, PlayRate);
}

bool UARPreviewAnimInstance::IsBlendingPose() const
{
    return GetProxyOnGameThread<FARPreviewAnimProxy>().Remaining > 0.f;
}
