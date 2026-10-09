#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MovieSceneSection.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

// Timing and track count of one animation, shared by the list and the single-animation reply.
static void DescribeWidgetAnimation(UWidgetAnimation* Anim, const TSharedPtr<FJsonObject>& Out)
{
    UMovieScene* MovieScene = Anim->MovieScene;
    if (!MovieScene)
    {
        return;
    }
    const FFrameRate FrameRate = MovieScene->GetTickResolution();
    const FFrameNumber Start = MovieScene->GetPlaybackRange().GetLowerBoundValue();
    const FFrameNumber End = MovieScene->GetPlaybackRange().GetUpperBoundValue();
    Out->SetNumberField(TEXT("durationSeconds"), FMath::RoundToDouble((End - Start).Value / FrameRate.AsDecimal() * 10000.0) / 10000.0);
    Out->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
    Out->SetNumberField(TEXT("startFrame"), Start.Value);
    Out->SetNumberField(TEXT("endFrame"), End.Value);
    // A widget animation keeps its tracks on the widgets it binds; counting only unbound tracks answered 0.
    int32 TrackCount = MCP_GET_MOVIESCENE_TRACKS(MovieScene).Num();
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        TrackCount += Binding.GetTracks().Num();
    }
    Out->SetNumberField(TEXT("trackCount"), TrackCount);
}

namespace
{
constexpr int32 McpMaxKeysPerChannel = 100;

// Channel names as add_animation_keyframe takes them: a transform's translation, angle, scale and shear, a colour's
// r, g, b, a; a single-channel track's is value. Null for any other, which is then named by its index.
const TCHAR* McpWidgetChannelName(const UMovieSceneTrack* Track, int32 Index, int32 Count)
{
    static const TCHAR* const Transform[] = {TEXT("translation.x"), TEXT("translation.y"), TEXT("angle"), TEXT("scale.x"),
                                             TEXT("scale.y"), TEXT("shear.x"), TEXT("shear.y")};
    static const TCHAR* const Color[] = {TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a")};
    if (Track->IsA<UMovieScene2DTransformTrack>() && Index < 7)
    {
        return Transform[Index];
    }
    if (Track->IsA<UMovieSceneColorTrack>() && Index < 4)
    {
        return Color[Index];
    }
    return Count == 1 ? TEXT("value") : nullptr;
}

// One widget track: the property it drives and every keyed channel as [seconds, value] pairs.
TSharedPtr<FJsonObject> McpDescribeWidgetTrack(UMovieSceneTrack* Track, const FFrameRate& Resolution)
{
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    const UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track);
    Out->SetStringField(TEXT("property"), PropertyTrack ? PropertyTrack->GetPropertyName().ToString() : Track->GetTrackName().ToString());
    Out->SetStringField(TEXT("kind"), Track->IsA<UMovieScene2DTransformTrack>() ? FString(TEXT("transform"))
                                      : Track->IsA<UMovieSceneColorTrack>()     ? FString(TEXT("color"))
                                      : Track->IsA<UMovieSceneFloatTrack>()     ? FString(TEXT("float"))
                                                                                : Track->GetClass()->GetName());
    const TSharedPtr<FJsonObject> Channels = MakeShared<FJsonObject>();
    // ponytail: every section's keys land under one channel name; key them per section if split tracks show up.
    for (UMovieSceneSection* Section : Track->GetAllSections())
    {
        const int32 Count = Section ? Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>().Num() : 0;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(Index);
            auto Data = Channel->GetData();
            TArray<TSharedPtr<FJsonValue>> Keys;
            for (int32 Key = 0; Key < FMath::Min(Data.GetTimes().Num(), McpMaxKeysPerChannel); ++Key)
            {
                Keys.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                    MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Resolution.AsSeconds(Data.GetTimes()[Key]) * 1000.0) / 1000.0),
                    MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Data.GetValues()[Key].Value * 10000.0) / 10000.0)}));
            }
            if (Keys.Num() > 0)
            {
                const TCHAR* Name = McpWidgetChannelName(Track, Index, Count);
                Channels->SetArrayField(Name ? FString(Name) : FString::FromInt(Index), Keys);
            }
        }
    }
    Out->SetObjectField(TEXT("channels"), Channels);
    return Out;
}

// Each widget the animation drives, by its name in the widget tree, with its tracks.
TArray<TSharedPtr<FJsonValue>> McpDescribeAnimatedWidgets(UWidgetAnimation* Anim)
{
    TArray<TSharedPtr<FJsonValue>> Widgets;
    const FFrameRate Resolution = Anim->MovieScene->GetTickResolution();
    for (const FMovieSceneBinding& Binding : Anim->MovieScene->GetBindings())
    {
        const FWidgetAnimationBinding* Bound = Anim->AnimationBindings.FindByPredicate(
            [&Binding](const FWidgetAnimationBinding& Candidate) { return Candidate.AnimationGuid == Binding.GetObjectGuid(); });
        TArray<TSharedPtr<FJsonValue>> Tracks;
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (Track)
            {
                Tracks.Add(MakeShared<FJsonValueObject>(McpDescribeWidgetTrack(Track, Resolution)));
            }
        }
        const TSharedPtr<FJsonObject> Widget = MakeShared<FJsonObject>();
        Widget->SetStringField(TEXT("widget"), Bound ? Bound->WidgetName.ToString() : Binding.GetName());
        Widget->SetArrayField(TEXT("tracks"), Tracks);
        Widgets.Add(MakeShared<FJsonValueObject>(Widget));
    }
    return Widgets;
}
} // namespace

bool HandleWidgetAuthoringAnimationQueries(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("get_widget_animation"), ESearchCase::IgnoreCase) || SubAction.Equals(TEXT("get_animation_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        if (AnimationName.IsEmpty())
        {
            TArray<TSharedPtr<FJsonValue>> AnimationsArray;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim)
                {
                    TSharedPtr<FJsonObject> AnimInfo = McpHandlerUtils::CreateResultObject();
                    AnimInfo->SetStringField(TEXT("name"), Anim->GetName());
                    DescribeWidgetAnimation(Anim, AnimInfo);
                    AnimationsArray.Add(MakeShared<FJsonValueObject>(AnimInfo));
                }
            }
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetArrayField(TEXT("animations"), AnimationsArray);
            ResultJson->SetNumberField(TEXT("animationCount"), WidgetBP->Animations.Num());
        }
        else
        {
            UWidgetAnimation* TargetAnim = WidgetAuthoringHelpers::FindWidgetAnimation(WidgetBP, AnimationName);

            if (!TargetAnim)
            {
                TArray<FString> Names;
                for (const UWidgetAnimation* Anim : WidgetBP->Animations)
                {
                    if (Anim)
                    {
                        Names.Add(Anim->GetName());
                    }
                }
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found; %s has: %s"), *AnimationName,
                    *WidgetBP->GetName(), Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("no animations")), TEXT("NOT_FOUND"));
                return true;
            }

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);

            DescribeWidgetAnimation(TargetAnim, ResultJson);
            if (TargetAnim->MovieScene)
            {
                // The tracks live on the widgets the animation binds; the unbound list this read gave was empty.
                ResultJson->SetArrayField(TEXT("widgets"), McpDescribeAnimatedWidgets(TargetAnim));
            }
        }

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved animation info"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("delete_animation"), ESearchCase::IgnoreCase))
    {
        const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        UWidgetBlueprint* WidgetBP = nullptr;
        UWidgetAnimation* Animation = ResolveWidgetAnimation(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
        if (!Animation)
        {
            return true;
        }

        WidgetBP->Animations.Remove(Animation);
        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("deletedAnimation"), AnimationName);
        ResultJson->SetNumberField(TEXT("remainingAnimations"), WidgetBP->Animations.Num());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Deleted animation"), ResultJson);
        return true;
    }

    return false;
}
}
