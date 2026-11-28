#pragma once

#include "Animation/AnimSingleNodeInstance.h"
#include "ARPreviewAnimInstance.generated.h"

UCLASS(Transient, NotBlueprintable)
class UARPreviewAnimInstance : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
public:
    void BlendTo(UAnimationAsset* Asset, bool bLoop, float PlayRate, float BlendSeconds);
    bool IsBlendingPose() const;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
