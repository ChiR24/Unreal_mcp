#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "MovieScene.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAnimationCore(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_widget_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"), TEXT("NewAnimation"));
        double Duration = GetJsonNumberField(Payload, TEXT("duration"), 1.0);

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

        if (WidgetAuthoringHelpers::FindWidgetAnimation(WidgetBP, AnimationName))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Animation '%s' already exists"), *AnimationName),
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UWidgetAnimation* NewAnim = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
        if (!NewAnim)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation"), TEXT("CREATE_FAILED"));
            return true;
        }

        // CRITICAL: Create and assign MovieScene immediately - GetMovieScene() returns nullptr until we do this
        // This matches the engine's pattern in AnimationTabSummoner.cpp
        NewAnim->MovieScene = NewObject<UMovieScene>(NewAnim, FName(*AnimationName), RF_Transactional);
        if (!NewAnim->MovieScene)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation MovieScene"), TEXT("CREATE_FAILED"));
            return true;
        }

        UMovieScene* MovieScene = NewAnim->GetMovieScene();

        // Clamp duration to avoid zero-length animations
        const double SafeDuration = FMath::Max(Duration, 0.01);

        // Set display rate (20 fps is the UE default for widget animations)
        MovieScene->SetDisplayRate(FFrameRate(20, 1));

        const FFrameTime InFrame = 0.0 * MovieScene->GetTickResolution();
        const FFrameTime OutFrame = SafeDuration * MovieScene->GetTickResolution();
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>(InFrame.FrameNumber, OutFrame.FrameNumber + 1));

        // CRITICAL: Register animation GUID and add to Animations array
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 805
        RegisterAnimationGuid(WidgetBP, NewAnim);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("duration"), SafeDuration);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget animation created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_animation_track"), ESearchCase::IgnoreCase))
    {
        const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        const FString SlotName = GetSlotName(Payload);
        const FString TrackType = GetJsonStringField(Payload, TEXT("trackType"), TEXT("opacity"));
        if (SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = nullptr;
        UWidgetAnimation* Animation = ResolveWidgetAnimation(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
        if (!Animation)
        {
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree ? FindWidgetByName(WidgetBP->WidgetTree, SlotName) : nullptr;
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found in tree"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // The binding and the property track are found or created, so a repeat call adds
        // nothing (it used to append a second possessable and binding every time).
        FMcpWidgetKeyResult Track;
        FString TrackError;
        if (!McpAddWidgetAnimationTrack(Animation, TargetWidget, TrackType, Track, TrackError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TrackError, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(WidgetBP));
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("trackType"), Track.TrackType);
        ResultJson->SetStringField(TEXT("propertyName"), Track.PropertyName);
        ResultJson->SetStringField(TEXT("trackClass"), Track.TrackClass);
        ResultJson->SetStringField(TEXT("bindingGuid"), Track.BindingGuid);
        ResultJson->SetBoolField(TEXT("bindingCreated"), Track.bCreatedBinding);
        ResultJson->SetBoolField(TEXT("trackCreated"), Track.bCreatedTrack);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s track %s on '%s' in '%s'"),
            *Track.PropertyName, Track.bCreatedTrack ? TEXT("added") : TEXT("already present"), *SlotName, *AnimationName), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_animation_keyframe"), ESearchCase::IgnoreCase))
    {
        return HandleWidgetAuthoringAnimationKeyframe(Subsystem, RequestId, Payload, RequestingSocket, ResultJson);
    }
    return false;
}
}
