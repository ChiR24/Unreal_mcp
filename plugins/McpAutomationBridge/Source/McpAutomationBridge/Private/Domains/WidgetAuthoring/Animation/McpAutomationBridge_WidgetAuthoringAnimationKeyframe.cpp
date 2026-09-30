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
    const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
    const double Time = GetJsonNumberField(Payload, TEXT("time"), 0.0);
    UWidgetBlueprint* WidgetBP = nullptr;
    UWidgetAnimation* Animation = ResolveWidgetAnimation(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
    if (!Animation)
    {
        return true;
    }
    FString SlotName = GetSlotName(Payload);
    if (SlotName.IsEmpty() && Animation->AnimationBindings.Num() > 0)
    {
        SlotName = Animation->AnimationBindings[0].WidgetName.ToString();
    }
    UWidget* TargetWidget = WidgetBP->WidgetTree && !SlotName.IsEmpty() ? FindWidgetByName(WidgetBP->WidgetTree, SlotName) : nullptr;
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
    // Names the Widget Blueprint so the receipt lists it: a saved key answered changes [].
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Keyframe added at %.3fs on %s.%s (%d key%s in the track)"), Time, *TargetWidget->GetName(),
                        *KeyResult.PropertyName, KeyResult.KeyCount, KeyResult.KeyCount == 1 ? TEXT("") : TEXT("s")),
        ResultJson);
    return true;
}
} // namespace WidgetAuthoringHandlers
