// McpAutomationBridge_WidgetAuthoringAnimationKeyframe.cpp — add_animation_keyframe (dogfood #38).
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeOperations.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAnimationKeyframe(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                            const TSharedPtr<FJsonObject>& Payload,
                                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                            TSharedPtr<FJsonObject> ResultJson)
{
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
    const double Time = GetJsonNumberField(Payload, TEXT("time"), 0.0);
    if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
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
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Animation '%s' not found; create it with create_widget_animation first"), *AnimationName),
            TEXT("ANIMATION_NOT_FOUND"));
        return true;
    }
    FString SlotName = GetSlotName(Payload);
    if (SlotName.IsEmpty() && Animation->AnimationBindings.Num() > 0)
    {
        SlotName = Animation->AnimationBindings[0].WidgetName.ToString();
    }
    UWidget* TargetWidget = nullptr;
    if (WidgetBP->WidgetTree && !SlotName.IsEmpty())
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
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Widget '%s' not found in the tree; pass slotName (the widget to animate)"), *SlotName),
            TEXT("WIDGET_NOT_FOUND"));
        return true;
    }
    FMcpWidgetKeyResult KeyResult;
    FString KeyError;
    FString KeyErrorCode;
    if (!McpAuthorWidgetAnimationKey(WidgetBP, Animation, TargetWidget, Payload, KeyResult, KeyError, KeyErrorCode))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, KeyError, KeyErrorCode);
        return true;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    const bool bSaved = McpSafeOperations::McpSafeAssetSave(WidgetBP);
    ResultJson->SetStringField(TEXT("animationName"), AnimationName);
    ResultJson->SetStringField(TEXT("slotName"), TargetWidget->GetName());
    ResultJson->SetStringField(TEXT("trackType"), KeyResult.TrackType);
    ResultJson->SetStringField(TEXT("propertyName"), KeyResult.PropertyName);
    ResultJson->SetStringField(TEXT("trackClass"), KeyResult.TrackClass);
    ResultJson->SetNumberField(TEXT("time"), Time);
    ResultJson->SetNumberField(TEXT("frameNumber"), KeyResult.FrameNumber);
    ResultJson->SetNumberField(TEXT("keyCount"), KeyResult.KeyCount);
    ResultJson->SetNumberField(TEXT("channelCount"), KeyResult.ChannelCount);
    ResultJson->SetBoolField(TEXT("createdTrack"), KeyResult.bCreatedTrack);
    ResultJson->SetBoolField(TEXT("createdBinding"), KeyResult.bCreatedBinding);
    ResultJson->SetStringField(TEXT("bindingGuid"), KeyResult.BindingGuid);
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Keyframe added at %.3fs on %s.%s (%d key%s in the track)"), Time, *TargetWidget->GetName(),
                        *KeyResult.PropertyName, KeyResult.KeyCount, KeyResult.KeyCount == 1 ? TEXT("") : TEXT("s")),
        ResultJson);
    return true;
}
} // namespace WidgetAuthoringHandlers
