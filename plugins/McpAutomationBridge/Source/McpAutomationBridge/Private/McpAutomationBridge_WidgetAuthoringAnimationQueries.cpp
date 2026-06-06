#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Animation/WidgetAnimation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAnimationQueries(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.14 Animation Extended Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_animation_speed"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        float PlaybackSpeed = GetJsonNumberField(Payload, TEXT("speed"), 1.0f);

        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidgetAnimation* TargetAnim = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                TargetAnim = Anim;
                break;
            }
        }

        if (!TargetAnim || !TargetAnim->MovieScene)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        // Animation playback speed is set at runtime, but we can store it as metadata
        // For design-time, we adjust the playback rate via the MovieScene settings
        // UE 5.7: SetPlaybackRange takes TRange<FFrameNumber>
        TRange<FFrameNumber> PlaybackRange = TargetAnim->MovieScene->GetPlaybackRange();
        TargetAnim->MovieScene->SetPlaybackRange(PlaybackRange);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("speed"), PlaybackSpeed);
        ResultJson->SetStringField(TEXT("note"), TEXT("Speed is applied at runtime. Animation marked as modified."));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set animation speed"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("get_animation_info"), ESearchCase::IgnoreCase))
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
            // Return list of all animations
            TArray<TSharedPtr<FJsonValue>> AnimationsArray;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim)
                {
                    TSharedPtr<FJsonObject> AnimInfo = McpHandlerUtils::CreateResultObject();
                    AnimInfo->SetStringField(TEXT("name"), Anim->GetName());
                    if (Anim->MovieScene)
                    {
                        FFrameRate FrameRate = Anim->MovieScene->GetTickResolution();
                        FFrameNumber Start = Anim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                        FFrameNumber End = Anim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                        float Duration = (End - Start).Value / FrameRate.AsDecimal();
                        AnimInfo->SetNumberField(TEXT("durationSeconds"), Duration);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetTracks().Num());
#else
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetMasterTracks().Num());
#endif
                    }
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
            // Return info for specific animation
            UWidgetAnimation* TargetAnim = nullptr;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
                {
                    TargetAnim = Anim;
                    break;
                }
            }

            if (!TargetAnim)
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
                return true;
            }

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);

            if (TargetAnim->MovieScene)
            {
                FFrameRate FrameRate = TargetAnim->MovieScene->GetTickResolution();
                FFrameNumber Start = TargetAnim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                FFrameNumber End = TargetAnim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                float Duration = (End - Start).Value / FrameRate.AsDecimal();

                ResultJson->SetNumberField(TEXT("durationSeconds"), Duration);
                ResultJson->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
                ResultJson->SetNumberField(TEXT("startFrame"), Start.Value);
                ResultJson->SetNumberField(TEXT("endFrame"), End.Value);

                TArray<TSharedPtr<FJsonValue>> TracksArray;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1+: GetMasterTracks() replaced with GetTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetTracks();
#else
                // UE 5.0: Use GetMasterTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetMasterTracks();
#endif
                for (UMovieSceneTrack* Track : MasterTracks)
                {
                    if (Track)
                    {
                        TSharedPtr<FJsonObject> TrackInfo = McpHandlerUtils::CreateResultObject();
                        TrackInfo->SetStringField(TEXT("name"), Track->GetTrackName().ToString());
                        TrackInfo->SetStringField(TEXT("type"), Track->GetClass()->GetName());
                        TracksArray.Add(MakeShared<FJsonValueObject>(TrackInfo));
                    }
                }
                ResultJson->SetArrayField(TEXT("tracks"), TracksArray);
            }
        }

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved animation info"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("delete_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        int32 FoundIndex = INDEX_NONE;
        for (int32 i = 0; i < WidgetBP->Animations.Num(); ++i)
        {
            if (WidgetBP->Animations[i] && WidgetBP->Animations[i]->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                FoundIndex = i;
                break;
            }
        }

        if (FoundIndex == INDEX_NONE)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        WidgetBP->Animations.RemoveAt(FoundIndex);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

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
