#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersKeyResolution.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersMappingSummaries.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpInputHandlers
{
UClass* ResolveInputClass(const FString& Name, const TCHAR* Prefix, UClass* Base)
{
    UClass* Class = Name.IsEmpty() ? nullptr : ResolveClassByName(Name.StartsWith(Prefix) ? Name : Prefix + Name);
    return Class && Class->IsChildOf(Base) && !Class->HasAnyClassFlags(CLASS_Abstract) ? Class : nullptr;
}

FEnhancedActionKeyMapping* FindInputMapping(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
{
    for (int32 Index = 0; Index < Context->GetMappings().Num(); ++Index)
    {
        FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Index);
        if (Mapping.Action == Action && Mapping.Key == Key)
        {
            return &Mapping;
        }
    }
    return nullptr;
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

    UClass* TriggerClass = ResolveInputClass(TriggerType, TEXT("InputTrigger"), UInputTrigger::StaticClass());
    if (!TriggerClass)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Unknown trigger type: %s. Name an Enhanced Input trigger class, with or without its InputTrigger prefix (Pressed, Released, Down, Tap, Hold, HoldAndRelease, Pulse, ...)."), *TriggerType),
            TEXT("INVALID_TRIGGER_TYPE"));
        return true;
    }

    InAction->Modify();
    InAction->Triggers.Add(NewObject<UInputTrigger>(InAction, TriggerClass));
    SaveLoadedAssetThrottled(InAction, true);

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

        TargetMapping = FindInputMapping(Context, InAction, RequestedKey);
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

    UClass* ModifierClass = ResolveInputClass(ModifierType, TEXT("InputModifier"), UInputModifier::StaticClass());
    if (!ModifierClass)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Unknown modifier type: %s. Name an Enhanced Input modifier class, with or without its InputModifier prefix (DeadZone, Negate, Scalar, SwizzleAxis, Smooth, ToWorldSpace, ...)."), *ModifierType),
            TEXT("INVALID_MODIFIER_TYPE"));
        return true;
    }
    UInputModifier* NewModifier = NewObject<UInputModifier>(ModifierOuter, ModifierClass);

    if (TargetMapping)
    {
        Context->Modify();
        TargetMapping->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(Context, true);
    }
    else
    {
        InAction->Modify();
        InAction->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(InAction, true);
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
}
