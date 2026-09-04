#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

namespace {

void AddNotifyList(const UAnimSequenceBase* Asset, const TSharedPtr<FJsonObject>& Response)
{
    TArray<TSharedPtr<FJsonValue>> Notifies;
    for (const FAnimNotifyEvent& Event : Asset->Notifies)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Event.NotifyName.ToString());
        Entry->SetNumberField(TEXT("time"), Event.GetTime());
        Entry->SetNumberField(TEXT("trackIndex"), Event.TrackIndex);
        Entry->SetNumberField(TEXT("duration"), Event.GetDuration());
        const UObject* NotifyObject = Event.Notify;
        if (!NotifyObject)
        {
            NotifyObject = Event.NotifyStateClass;
        }
        Entry->SetStringField(TEXT("notifyClass"), NotifyObject ? NotifyObject->GetClass()->GetName() : FString());
        Notifies.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Response->SetArrayField(TEXT("notifies"), Notifies);
    Response->SetNumberField(TEXT("numNotifies"), Notifies.Num());
}

void AddCurveNames(const UAnimSequenceBase* Asset, const TSharedPtr<FJsonObject>& Response)
{
    TArray<TSharedPtr<FJsonValue>> Curves;
    for (const FFloatCurve& Curve : Asset->GetCurveData().FloatCurves)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        Curves.Add(MakeShared<FJsonValueString>(Curve.GetName().ToString()));
#else
        Curves.Add(MakeShared<FJsonValueString>(Curve.Name.DisplayName.ToString()));
#endif
    }
    Response->SetArrayField(TEXT("curves"), Curves);
    Response->SetNumberField(TEXT("numCurves"), Curves.Num());
}

// Fields shared by every UAnimSequenceBase (sequences, montages, composites).
void AddSequenceBaseInfo(const UAnimSequenceBase* Asset, const TSharedPtr<FJsonObject>& Response)
{
    if (const USkeleton* Skeleton = Asset->GetSkeleton())
    {
        Response->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
    }
    const double Length = Asset->GetPlayLength();
    Response->SetNumberField(TEXT("length"), Length);
    Response->SetNumberField(TEXT("duration"), Length);
#if ENGINE_MAJOR_VERSION >= 5
    Response->SetNumberField(TEXT("frameRate"), Asset->GetSamplingFrameRate().AsDecimal());
    Response->SetNumberField(TEXT("numFrames"), Asset->GetNumberOfSampledKeys());
#endif
    Response->SetNumberField(TEXT("rateScale"), Asset->RateScale);
    AddNotifyList(Asset, Response);
    AddCurveNames(Asset, Response);
}

void AddMontageInfo(const UAnimMontage* Montage, const TSharedPtr<FJsonObject>& Response)
{
    TArray<TSharedPtr<FJsonValue>> Sections;
    for (const FCompositeSection& Section : Montage->CompositeSections)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Section.SectionName.ToString());
        Entry->SetNumberField(TEXT("startTime"), Section.GetTime());
        Entry->SetStringField(TEXT("nextSection"), Section.NextSectionName.ToString());
        Sections.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Response->SetArrayField(TEXT("sections"), Sections);
    Response->SetNumberField(TEXT("numSections"), Sections.Num());

    TArray<TSharedPtr<FJsonValue>> Slots;
    for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
    {
        Slots.Add(MakeShared<FJsonValueString>(Track.SlotName.ToString()));
    }
    Response->SetArrayField(TEXT("slots"), Slots);
    Response->SetNumberField(TEXT("numSlots"), Slots.Num());
    Response->SetNumberField(TEXT("blendIn"), Montage->BlendIn.GetBlendTime());
    Response->SetNumberField(TEXT("blendOut"), Montage->BlendOut.GetBlendTime());
    Response->SetNumberField(TEXT("blendOutTriggerTime"), Montage->BlendOutTriggerTime);
}

void AddBlendSpaceInfo(const UBlendSpace* BlendSpace, bool bIs1D, const TSharedPtr<FJsonObject>& Response)
{
    if (const USkeleton* Skeleton = BlendSpace->GetSkeleton())
    {
        Response->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
    }
    TArray<TSharedPtr<FJsonValue>> Axes;
    const int32 AxisCount = bIs1D ? 1 : 2;
    for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
    {
        const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(AxisIndex);
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Parameter.DisplayName);
        Entry->SetNumberField(TEXT("min"), Parameter.Min);
        Entry->SetNumberField(TEXT("max"), Parameter.Max);
        Entry->SetNumberField(TEXT("gridNum"), Parameter.GridNum);
        Axes.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Response->SetArrayField(TEXT("axes"), Axes);
    Response->SetNumberField(TEXT("numAxes"), AxisCount);
    const int32 SampleCount = BlendSpace->GetBlendSamples().Num();
    Response->SetNumberField(TEXT("sampleCount"), SampleCount);
    Response->SetNumberField(TEXT("numSamples"), SampleCount);
}

} // namespace

TSharedPtr<FJsonObject> HandleAnimationInfoActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction != TEXT("get_animation_info"))
    {
        return nullptr;
    }

    const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    // The contract declares length/frameRate at the top level of the result;
    // a nested animationInfo object never surfaced them (dogfood #83).
    Response->SetStringField(TEXT("assetPath"), Asset->GetPathName());
    Response->SetStringField(TEXT("className"), Asset->GetClass()->GetName());
    if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
    {
        Response->SetStringField(TEXT("assetType"), TEXT("AnimSequence"));
        AddSequenceBaseInfo(Sequence, Response);
        const bool bAdditive = Sequence->AdditiveAnimType != AAT_None;
        Response->SetBoolField(TEXT("additive"), bAdditive);
        Response->SetBoolField(TEXT("isAdditive"), bAdditive);
        Response->SetBoolField(TEXT("hasRootMotion"), Sequence->bEnableRootMotion);
    }
    else if (const UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
    {
        Response->SetStringField(TEXT("assetType"), TEXT("AnimMontage"));
        AddSequenceBaseInfo(Montage, Response);
        AddMontageInfo(Montage, Response);
    }
    else if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
    {
        const bool bIs1D = Asset->IsA<UBlendSpace1D>();
        Response->SetStringField(TEXT("assetType"), bIs1D ? TEXT("BlendSpace1D") : TEXT("BlendSpace2D"));
        AddBlendSpaceInfo(BlendSpace, bIs1D, Response);
    }
    else if (const UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset))
    {
        Response->SetStringField(TEXT("assetType"), TEXT("AnimBlueprint"));
        if (AnimBP->TargetSkeleton)
        {
            Response->SetStringField(TEXT("skeletonPath"), AnimBP->TargetSkeleton->GetPathName());
        }
        Response->SetStringField(TEXT("parentClass"), AnimBP->ParentClass ? AnimBP->ParentClass->GetName() : FString());
    }
    else
    {
        Response->SetStringField(TEXT("assetType"), Asset->GetClass()->GetName());
    }

    ANIM_SUCCESS_RESPONSE(TEXT("Animation info retrieved"));
    McpHandlerUtils::AddVerification(Response, Asset);
    return Response;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
