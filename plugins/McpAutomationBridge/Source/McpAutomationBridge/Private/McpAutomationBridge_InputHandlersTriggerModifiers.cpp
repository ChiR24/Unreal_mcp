#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InputHandlersAssetResolution.h"
#include "McpAutomationBridge_InputHandlersKeyResolution.h"
#include "McpAutomationBridge_InputHandlersMappingSummaries.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
namespace
{
UInputModifier* CreateInputModifierForType(const FString& ModifierType, UObject* Outer)
{
    if (ModifierType == TEXT("DeadZone") || ModifierType == TEXT("InputModifierDeadZone"))
    {
        return NewObject<UInputModifierDeadZone>(Outer);
    }
    if (ModifierType == TEXT("SmoothDelta") || ModifierType == TEXT("InputModifierSmoothDelta"))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        return NewObject<UInputModifierSmoothDelta>(Outer);
#else
        return NewObject<UInputModifierSmooth>(Outer);
#endif
    }
    if (ModifierType == TEXT("SwizzleInputAxis") || ModifierType == TEXT("InputModifierSwizzleAxis"))
    {
        return NewObject<UInputModifierSwizzleAxis>(Outer);
    }
    if (ModifierType == TEXT("Negate") || ModifierType == TEXT("InputModifierNegate"))
    {
        return NewObject<UInputModifierNegate>(Outer);
    }
    if (ModifierType == TEXT("Scalar") || ModifierType == TEXT("InputModifierScalar"))
    {
        return NewObject<UInputModifierScalar>(Outer);
    }
    if (ModifierType == TEXT("ScaleByDeltaTime") || ModifierType == TEXT("InputModifierScaleByDeltaTime"))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        return NewObject<UInputModifierScaleByDeltaTime>(Outer);
#else
        return NewObject<UInputModifierScalar>(Outer);
#endif
    }
    if (ModifierType == TEXT("ToWorldSpace") || ModifierType == TEXT("InputModifierToWorldSpace"))
    {
        return NewObject<UInputModifierToWorldSpace>(Outer);
    }

    return nullptr;
}
}

bool HandleSetInputTrigger(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString TriggerType;
    Payload->TryGetStringField(TEXT("triggerType"), TriggerType);

    FString SanitizedActionPath;
    UInputAction* InAction = LoadInputActionAsset(ActionPath, SanitizedActionPath);
    if (!InAction)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    UInputTrigger* NewTrigger = nullptr;
    if (TriggerType == TEXT("Pressed") || TriggerType == TEXT("InputTriggerPressed"))
    {
        NewTrigger = NewObject<UInputTriggerPressed>(InAction);
    }
    else if (TriggerType == TEXT("Released") || TriggerType == TEXT("InputTriggerReleased"))
    {
        NewTrigger = NewObject<UInputTriggerReleased>(InAction);
    }
    else if (TriggerType == TEXT("Down") || TriggerType == TEXT("InputTriggerDown"))
    {
        NewTrigger = NewObject<UInputTriggerDown>(InAction);
    }
    else if (TriggerType == TEXT("Tap") || TriggerType == TEXT("InputTriggerTap"))
    {
        NewTrigger = NewObject<UInputTriggerTap>(InAction);
    }
    else if (TriggerType == TEXT("Hold") || TriggerType == TEXT("InputTriggerHold"))
    {
        NewTrigger = NewObject<UInputTriggerHold>(InAction);
    }
    else if (TriggerType == TEXT("HoldAndRelease") || TriggerType == TEXT("InputTriggerHoldAndRelease"))
    {
        NewTrigger = NewObject<UInputTriggerHoldAndRelease>(InAction);
    }
    else if (TriggerType == TEXT("Pulse") || TriggerType == TEXT("InputTriggerPulse"))
    {
        NewTrigger = NewObject<UInputTriggerPulse>(InAction);
    }
    else if (TriggerType == TEXT("RepeatedTap") || TriggerType == TEXT("InputTriggerRepeatedTap") || TriggerType == TEXT("DoubleTap"))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
        UInputTriggerRepeatedTap* RepeatedTapTrigger = NewObject<UInputTriggerRepeatedTap>(InAction);
        NewTrigger = RepeatedTapTrigger;
#else
        NewTrigger = NewObject<UInputTriggerTap>(InAction);
#endif
    }

    if (!NewTrigger)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Unknown trigger type: %s. Supported: Pressed, Released, Down, Tap, Hold, HoldAndRelease, Pulse, RepeatedTap, DoubleTap"), *TriggerType),
            TEXT("INVALID_TRIGGER_TYPE"));
        return true;
    }

    InAction->Triggers.Add(NewTrigger);
    SaveLoadedAssetThrottled(InAction, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("triggerType"), TriggerType);
    Result->SetBoolField(TEXT("triggerSet"), true);
    McpHandlerUtils::AddVerification(Result, InAction);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Trigger '%s' configured on action."), *TriggerType), Result);
    return true;
}

bool HandleSetInputModifier(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);
    FString ModifierType;
    Payload->TryGetStringField(TEXT("modifierType"), ModifierType);

    const bool bTargetMapping = !ContextPath.IsEmpty() || !KeyName.IsEmpty();
    if (bTargetMapping && (ContextPath.IsEmpty() || KeyName.IsEmpty()))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("contextPath and key are both required when setting a modifier on a specific mapping."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedActionPath;
    UInputAction* InAction = LoadInputActionAsset(ActionPath, SanitizedActionPath);
    if (!InAction)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    UObject* ModifierOuter = InAction;
    UInputMappingContext* Context = nullptr;
    FString SanitizedContextPath;
    FEnhancedActionKeyMapping* TargetMapping = nullptr;
    FKey RequestedKey = InputKeyFromName(KeyName);
    if (bTargetMapping)
    {
        if (!RequestedKey.IsValid())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid key name: %s"), *KeyName), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        Context = LoadInputMappingContextAsset(ContextPath, SanitizedContextPath);
        if (!Context)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Context not found: %s"), *SanitizedContextPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        const int32 MappingCount = Context->GetMappings().Num();
        for (int32 MappingIndex = 0; MappingIndex < MappingCount; ++MappingIndex)
        {
            FEnhancedActionKeyMapping& Mapping = Context->GetMapping(MappingIndex);
            if (Mapping.Action == InAction && Mapping.Key == RequestedKey)
            {
                TargetMapping = &Mapping;
                break;
            }
        }

        if (!TargetMapping)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Mapping not found for action '%s' and key '%s'."),
                    *SanitizedActionPath, *KeyName),
                TEXT("NOT_FOUND"));
            return true;
        }

        ModifierOuter = Context;
    }

    UInputModifier* NewModifier = CreateInputModifierForType(ModifierType, ModifierOuter);
    if (!NewModifier)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Unknown modifier type: %s. Supported: DeadZone, SmoothDelta, SwizzleInputAxis, Negate, Scalar, ScaleByDeltaTime, ToWorldSpace"), *ModifierType),
            TEXT("INVALID_MODIFIER_TYPE"));
        return true;
    }

    if (TargetMapping)
    {
        Context->Modify();
        TargetMapping->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(Context, -1.0, true);
    }
    else
    {
        InAction->Modify();
        InAction->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(InAction, -1.0, true);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("modifierType"), ModifierType);
    Result->SetBoolField(TEXT("modifierSet"), true);
    Result->SetStringField(TEXT("target"), TargetMapping ? TEXT("mapping") : TEXT("action"));
    if (TargetMapping)
    {
        Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
        Result->SetStringField(TEXT("key"), KeyName);
        Result->SetNumberField(TEXT("mappingModifierCount"), TargetMapping->Modifiers.Num());
        AddInputMappingSummary(Result, Context, InAction);
        AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
        AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);
    }
    else
    {
        McpHandlerUtils::AddVerification(Result, InAction);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Modifier '%s' configured on action."), *ModifierType), Result);
    return true;
}
#endif
}
