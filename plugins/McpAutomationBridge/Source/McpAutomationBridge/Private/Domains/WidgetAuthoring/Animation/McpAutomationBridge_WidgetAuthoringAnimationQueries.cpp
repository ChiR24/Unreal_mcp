#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Animation/WidgetAnimation.h"
#include "Kismet2/BlueprintEditorUtils.h"
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
    Out->SetNumberField(TEXT("durationSeconds"), (End - Start).Value / FrameRate.AsDecimal());
    Out->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
    Out->SetNumberField(TEXT("startFrame"), Start.Value);
    Out->SetNumberField(TEXT("endFrame"), End.Value);
    Out->SetNumberField(TEXT("trackCount"), MCP_GET_MOVIESCENE_TRACKS(MovieScene).Num());
}

bool HandleWidgetAuthoringAnimationQueries(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
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
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
                return true;
            }

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);

            DescribeWidgetAnimation(TargetAnim, ResultJson);
            if (TargetAnim->MovieScene)
            {
                TArray<TSharedPtr<FJsonValue>> TracksArray;
                for (UMovieSceneTrack* Track : MCP_GET_MOVIESCENE_TRACKS(TargetAnim->MovieScene))
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
