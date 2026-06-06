#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleAnimationInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("get_animation_info"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));

        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
        if (!Asset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }

        TSharedPtr<FJsonObject> AnimInfo = McpHandlerUtils::CreateResultObject();

        if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimSequence"));
            if (Sequence->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Sequence->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Sequence->GetPlayLength());
#if ENGINE_MAJOR_VERSION >= 5
            AnimInfo->SetNumberField(TEXT("numFrames"), Sequence->GetNumberOfSampledKeys());
            AnimInfo->SetNumberField(TEXT("frameRate"), Sequence->GetSamplingFrameRate().AsDecimal());
#endif
            AnimInfo->SetNumberField(TEXT("numNotifies"), Sequence->Notifies.Num());
            AnimInfo->SetBoolField(TEXT("isAdditive"), Sequence->AdditiveAnimType != AAT_None);
            AnimInfo->SetBoolField(TEXT("hasRootMotion"), Sequence->bEnableRootMotion);
        }
        else if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimMontage"));
            if (Montage->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Montage->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
            AnimInfo->SetNumberField(TEXT("numSections"), Montage->CompositeSections.Num());
            AnimInfo->SetNumberField(TEXT("numSlots"), Montage->SlotAnimTracks.Num());
            AnimInfo->SetNumberField(TEXT("numNotifies"), Montage->Notifies.Num());
        }
        else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), Cast<UBlendSpace1D>(Asset) ? TEXT("BlendSpace1D") : TEXT("BlendSpace2D"));
            if (BlendSpace->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), BlendSpace->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("numSamples"), BlendSpace->GetBlendSamples().Num());
        }
        else if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimBlueprint"));
            if (AnimBP->TargetSkeleton)
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), AnimBP->TargetSkeleton->GetPathName());
            }
            AnimInfo->SetStringField(TEXT("parentClass"), AnimBP->ParentClass ? AnimBP->ParentClass->GetName() : TEXT(""));
        }
        else
        {
            AnimInfo->SetStringField(TEXT("assetType"), Asset->GetClass()->GetName());
        }

        Response->SetObjectField(TEXT("animationInfo"), AnimInfo);
        ANIM_SUCCESS_RESPONSE(TEXT("Animation info retrieved"));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
