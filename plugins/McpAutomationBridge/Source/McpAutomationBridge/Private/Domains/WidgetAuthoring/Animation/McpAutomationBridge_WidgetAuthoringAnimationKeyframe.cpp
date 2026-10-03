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

namespace
{
// One entry of keys: the call's own fields with the entry's on top, so an entry names only what
// differs from the others (its time and value, often its widget and track).
TSharedPtr<FJsonObject> MergeKey(const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Key)
{
    TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
    for (const auto& Pair : Payload->Values)
    {
        Merged->SetField(FString(Pair.Key.Len(), *Pair.Key), Pair.Value);
    }
    Merged->RemoveField(TEXT("keys"));
    for (const auto& Pair : Key->Values)
    {
        Merged->SetField(FString(Pair.Key.Len(), *Pair.Key), Pair.Value);
    }
    return Merged;
}

// Why the key's track or value would be refused (McpAuthorWidgetAnimationKey reads them the same way).
FString KeyRefusal(const TSharedPtr<FJsonObject>& Key)
{
    FString TrackType = GetJsonStringField(Key, TEXT("trackType"));
    TrackType = TrackType.IsEmpty() ? GetJsonStringField(Key, TEXT("propertyName")) : TrackType;
    TrackType = TrackType.IsEmpty() ? FString(TEXT("opacity")) : TrackType;
    return KeyValueError(TrackType.ToLower(), TrackType, ReadValueField(Key));
}
} // namespace

bool HandleWidgetAuthoringAnimationKeyframe(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                            const TSharedPtr<FJsonObject>& Payload,
                                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                            TSharedPtr<FJsonObject> ResultJson)
{
    const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
    UWidgetBlueprint* WidgetBP = nullptr;
    UWidgetAnimation* Animation = ResolveWidgetAnimation(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
    if (!Animation)
    {
        return true;
    }
    // keys: a whole animation (an entrance, a pulse) in one call. Every key is checked before any is
    // written, so a refused batch adds nothing; without keys the call is its own single key.
    const TArray<TSharedPtr<FJsonValue>>* KeyList = nullptr;
    const bool bBatch = Payload->TryGetArrayField(TEXT("keys"), KeyList);
    TArray<TSharedPtr<FJsonObject>> Keys;
    for (int32 Index = 0; bBatch && Index < KeyList->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* KeyObject = nullptr;
        Keys.Add((*KeyList)[Index]->TryGetObject(KeyObject) ? MergeKey(Payload, *KeyObject) : nullptr);
    }
    if (!bBatch)
    {
        Keys.Add(Payload);
    }
    if (Keys.Num() == 0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("keys is empty; give it one object per keyframe"),
                                      TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<UWidget*> Targets;
    for (int32 Index = 0; Index < Keys.Num(); ++Index)
    {
        const FString Where = bBatch ? FString::Printf(TEXT("keys[%d]: "), Index) : FString();
        FString SlotName = Keys[Index].IsValid() ? GetSlotName(Keys[Index]) : FString();
        if (SlotName.IsEmpty() && Animation->AnimationBindings.Num() > 0)
        {
            SlotName = Animation->AnimationBindings[0].WidgetName.ToString();
        }
        UWidget* Target = WidgetBP->WidgetTree && !SlotName.IsEmpty() ? FindWidgetByName(WidgetBP->WidgetTree, SlotName) : nullptr;
        const FString Refusal = !Keys[Index].IsValid() ? FString(TEXT("each key is an object such as {time, propertyValue}"))
            : !Target ? FString::Printf(TEXT("Widget '%s' not found in the tree; pass slotName (the widget to animate)"), *SlotName)
            : KeyRefusal(Keys[Index]);
        if (!Refusal.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, Where + Refusal,
                                          Keys[Index].IsValid() && !Target ? TEXT("WIDGET_NOT_FOUND") : TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Targets.Add(Target);
    }
    FMcpWidgetKeyResult KeyResult;
    for (int32 Index = 0; Index < Keys.Num(); ++Index)
    {
        FString KeyError;
        FString KeyErrorCode;
        if (!McpAuthorWidgetAnimationKey(WidgetBP, Animation, Targets[Index], Keys[Index], KeyResult, KeyError, KeyErrorCode))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                (bBatch ? FString::Printf(TEXT("keys[%d]: "), Index) : FString()) + KeyError, KeyErrorCode);
            return true;
        }
    }
    const double Time = GetJsonNumberField(Keys.Last(), TEXT("time"), 0.0);
    UWidget* TargetWidget = Targets.Last();
    if (bBatch)
    {
        ResultJson->SetNumberField(TEXT("keysAdded"), Keys.Num());
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
        bBatch ? FString::Printf(TEXT("%d keyframes added to %s, saved once"), Keys.Num(), *AnimationName)
               : FString::Printf(TEXT("Keyframe added at %.3fs on %s.%s (%d key%s in the track)"), Time, *TargetWidget->GetName(),
                                 *KeyResult.PropertyName, KeyResult.KeyCount, KeyResult.KeyCount == 1 ? TEXT("") : TEXT("s")),
        ResultJson);
    return true;
}
} // namespace WidgetAuthoringHandlers
