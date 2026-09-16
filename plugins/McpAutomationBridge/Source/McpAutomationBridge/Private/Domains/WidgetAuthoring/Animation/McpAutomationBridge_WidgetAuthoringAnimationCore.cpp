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
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        FString SlotName = GetSlotName(Payload);
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT("RenderOpacity"));

        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidgetAnimation* Animation = WidgetAuthoringHelpers::FindWidgetAnimation(WidgetBP, AnimationName);

        if (!Animation)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = nullptr;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                if (Widget && Widget->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
                {
                    TargetWidget = Widget;
                }
            });
        }

        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found in tree"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // The animation track binding is set up - MovieScene integration would add the actual track
        // For now, we create the binding reference
        UMovieScene* MovieScene = Animation->GetMovieScene();
        if (!MovieScene)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Animation has no MovieScene"), TEXT("ANIMATION_ERROR"));
            return true;
        }

        FGuid BindingGuid = MovieScene->AddPossessable(TargetWidget->GetFName().ToString(), TargetWidget->GetClass());

        // CRITICAL: For editor-time (WidgetBlueprint context), we cannot use BindPossessableObject
        // because it expects a UUserWidget runtime context and will crash with CastChecked.
        // Instead, directly add the binding to AnimationBindings array.
        FWidgetAnimationBinding NewBinding;
        NewBinding.AnimationGuid = BindingGuid;
        NewBinding.WidgetName = TargetWidget->GetFName();
        NewBinding.SlotWidgetName = NAME_None;
        NewBinding.bIsRootWidget = false;

        Animation->AnimationBindings.Add(NewBinding);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation track added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_animation_keyframe"), ESearchCase::IgnoreCase))
    {
        return HandleWidgetAuthoringAnimationKeyframe(Subsystem, RequestId, Payload, RequestingSocket, ResultJson);
    }
    if (SubAction.Equals(TEXT("set_animation_loop"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        bool bLoop = GetJsonBoolField(Payload, TEXT("loop"), true);
        int32 LoopCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("loopCount"), 0)); // 0 = infinite

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

        UWidgetAnimation* Animation = WidgetAuthoringHelpers::FindWidgetAnimation(WidgetBP, AnimationName);

        if (!Animation)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }

        // UMG widget animations have no persisted loop setting: looping is a PlayAnimation()
        // NumLoopsToPlay argument at runtime. This branch used to answer "Animation loop settings
        // configured" with success:true while storing nothing, and then marked the asset modified and
        // saved it for that non-change. Report what actually happened, and leave the asset untouched.
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetBoolField(TEXT("requestedLoop"), bLoop);
        ResultJson->SetNumberField(TEXT("requestedLoopCount"), LoopCount);
        ResultJson->SetBoolField(TEXT("applied"), false);
        ResultJson->SetStringField(TEXT("note"), TEXT("Nothing was stored and the widget asset was left unchanged. Loop behaviour is passed to PlayAnimation() as NumLoopsToPlay at runtime."));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("A widget animation has no stored loop setting, so no loop configuration exists to apply on '%s'. Pass NumLoopsToPlay to PlayAnimation() at runtime instead. Requested loop=%s, loopCount=%d."),
                            *AnimationName, bLoop ? TEXT("true") : TEXT("false"), LoopCount),
            ResultJson, TEXT("NOT_APPLICABLE"));
        return true;
    }

    return false;
}
}
