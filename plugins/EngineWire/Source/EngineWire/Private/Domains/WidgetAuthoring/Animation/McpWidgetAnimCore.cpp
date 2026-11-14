#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetAnimKeys.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetGuidRegistry.h"
#include "Domains/WidgetAuthoring/McpWidgetPayload.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "MovieScene.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAnimationCore(
    UEngineWireSubsystem& Subsystem,
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

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprintOrReply(Subsystem, RequestId, RequestingSocket, WidgetPath);
        if (!WidgetBP)
        {
            return true;
        }

        if (WidgetAuthoringHelpers::FindWidgetAnimation(WidgetBP, AnimationName))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Animation '%s' already exists"), *AnimationName),
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        // The editor's own check of a new member name (an empty one, characters a member cannot hold, a name a variable
        // or function has), then any object the Blueprint holds under it (EventGraph, WidgetTree, a function graph):
        // NewObject over one of another class is a Fatal log.
        FKismetNameValidator Validator(WidgetBP);
        const auto Validity = Validator.IsValid(AnimationName);
        const bool bHeld = Validity == EValidatorResult::AlreadyInUse || FindObject<UObject>(WidgetBP, *AnimationName) != nullptr;
        if (bHeld || Validity != EValidatorResult::Ok)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                bHeld ? FString::Printf(TEXT("'%s' already names a member of this widget; pick another animationName. Nothing was created."), *AnimationName)
                      : FString::Printf(TEXT("'%s' cannot name an animation: %s"), *AnimationName, *INameValidatorInterface::GetErrorString(AnimationName, Validity)),
                bHeld ? TEXT("ALREADY_EXISTS") : TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Made through the shared creation, which refuses a duration past what the scene's tick rate holds before
        // anything is made.
        const double SafeDuration = FMath::Max(Duration, 0.01);
        FString DurationError;
        UWidgetAnimation* NewAnim = McpCreateWidgetAnimation(WidgetBP, AnimationName, SafeDuration, DurationError);
        if (!NewAnim)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("duration %g s is too long for an animation: %s"), Duration, *DurationError),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        // The animation is a variable a graph reads (PlayAnimation's target): until a compile gave the generated
        // class its property, a VariableGet of it failed with "Variable not found".
        RefreshWidgetBlueprintClass(WidgetBP);
        ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(WidgetBP));

        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("duration"), SafeDuration);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));

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

        UWidget* TargetWidget = WidgetBP->WidgetTree ? WidgetBP->WidgetTree->FindWidget(FName(*SlotName)) : nullptr;
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
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("trackType"), Track.TrackType);
        ResultJson->SetStringField(TEXT("propertyName"), Track.PropertyName);
        ResultJson->SetStringField(TEXT("trackClass"), Track.TrackClass);
        ResultJson->SetStringField(TEXT("bindingGuid"), Track.BindingGuid);
        ResultJson->SetBoolField(TEXT("createdBinding"), Track.bCreatedBinding);
        ResultJson->SetBoolField(TEXT("createdTrack"), Track.bCreatedTrack);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
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
