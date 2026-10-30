// Copyright (c) 2024 EngineWire Contributors
//
// animation_physics add_footsteps: a Play Sound notify where each foot is set down in a walk or run cycle, found from
// the clip's own frames (a foot is down within contactHeight of its lowest point), in one undo step.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/Analysis/McpAnimLocomotion.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Sound/SoundBase.h"

namespace McpAnimationAuthoring
{
namespace
{
const FName FootstepNotifyName(TEXT("Footstep"));

struct FFootstep
{
    FName Foot;
    double Time = 0.0;
};

// The times each foot is set down: the first sample of every run of samples within ContactHeight of its lowest point.
// A run that holds the first sample continues the one the loop ends with when that is down too, so it is no new step.
TArray<FFootstep> StepsOf(const McpAnimLocomotion::FFeetSample& Sample, double ContactHeight)
{
    TArray<FFootstep> Steps;
    for (int32 Foot = 0; Foot < Sample.Feet.Num(); ++Foot)
    {
        const TArray<FVector>& Path = Sample.Paths[Foot];
        double Lowest = TNumericLimits<double>::Max();
        for (const FVector& Point : Path)
        {
            Lowest = FMath::Min(Lowest, Point.Z);
        }
        const auto Down = [&Path, Lowest, ContactHeight](int32 Index) { return Path[Index].Z <= Lowest + ContactHeight; };
        const bool bWraps = Path.Num() > 1 && Down(0) && Down(Path.Num() - 1);
        for (int32 Index = 0; Index < Path.Num(); ++Index)
        {
            const bool bStart = Down(Index) && (Index == 0 ? !bWraps : !Down(Index - 1));
            if (bStart)
            {
                Steps.Add({Sample.Feet[Foot], Sample.Times[Index]});
            }
        }
    }
    Steps.Sort([](const FFootstep& A, const FFootstep& B) { return A.Time < B.Time; });
    return Steps;
}
} // namespace

TSharedPtr<FJsonObject> HandleAddFootsteps(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
    if (!Sequence)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("%s is not an animation sequence."), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
    }
    const FString SoundPath = GetJsonStringField(Params, TEXT("soundPath"), TEXT(""));
    USoundBase* Sound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *NormalizeAnimPath(SoundPath)));
    if (!Sound)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("%s is not a sound (a Sound Wave, Sound Cue or MetaSound)."), *SoundPath), TEXT("SOUND_NOT_FOUND"));
    }
    const double ContactHeight = GetJsonNumberField(Params, TEXT("contactHeight"), 5.0);
    const McpAnimLocomotion::FFeetSample Sample = McpAnimLocomotion::SampleFeet(Sequence, Params);
    if (!Sample.ErrorCode.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(Sample.Error, Sample.ErrorCode);
    }
    if (Sample.Feet.Num() == 0)
    {
        ANIM_ERROR_RESPONSE(TEXT("No bone of this skeleton is named like a foot: name them in feet (list_bones lists them)."), TEXT("BONE_NOT_FOUND"));
    }
    const TArray<FFootstep> Steps = StepsOf(Sample, ContactHeight);
    // A run again replaces the footsteps an earlier run made, so they never double up.
    const auto IsFootstep = [](const FAnimNotifyEvent& Event)
        { return Event.NotifyName == FootstepNotifyName && Cast<UAnimNotify_PlaySound>(Event.Notify) != nullptr; };
    int32 Earlier = 0;
    for (const FAnimNotifyEvent& Event : Sequence->Notifies)
    {
        Earlier += IsFootstep(Event) ? 1 : 0;
    }
    // An idle with no footsteps yet changes nothing: no undo step, nothing saved.
    const bool bChanges = Steps.Num() > 0 || Earlier > 0;
    if (bChanges)
    {
        const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpFootsteps", "Add", "Add Footsteps"), EMcpMutationDurability::EditorStateOnly, {Sequence});
        Sequence->Modify();
        Sequence->Notifies.RemoveAll(IsFootstep);
        if (Steps.Num() > 0 && Sequence->AnimNotifyTracks.Num() == 0)
        {
            Sequence->InitializeNotifyTrack();
        }
        for (const FFootstep& Step : Steps)
        {
            UAnimNotify_PlaySound* Notify = NewObject<UAnimNotify_PlaySound>(Sequence, NAME_None, RF_Transactional);
            Notify->Sound = Sound;
            FAnimNotifyEvent& Event = Sequence->Notifies.AddDefaulted_GetRef();
            Event.Link(Sequence, static_cast<float>(Step.Time));
            Event.TrackIndex = 0;
            Event.NotifyName = FootstepNotifyName;
            Event.Notify = Notify;
        }
        Sequence->RefreshCacheData();
        Transaction.DescribeInto(Response);
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FFootstep& Step : Steps)
    {
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("foot"), Step.Foot.ToString());
        Row->SetNumberField(TEXT("time"), FMath::RoundToDouble(Step.Time * 1000.0) / 1000.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Response->SetArrayField(TEXT("steps"), Rows);
    Response->SetNumberField(TEXT("removed"), Earlier);
    Response->SetStringField(TEXT("soundPath"), Sound->GetPathName());
    SaveAnimAssetAndReport(Sequence, bChanges && GetJsonBoolField(Params, TEXT("save"), true), Response);
    McpHandlerUtils::AddVerification(Response, Sequence);
    if (!bChanges)
    {
        McpHandlerUtils::MarkNoAssetsChanged(Response);
    }
    if (Steps.Num() == 0)
    {
        Response->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray({TEXT("No foot is lifted and set down in this clip "
            "(an idle keeps both down), so no footstep was added.")}));
    }
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("%d footstep(s) play %s in %s."), Steps.Num(), *Sound->GetName(), *Sequence->GetName()));
    return Response;
}
} // namespace McpAnimationAuthoring
