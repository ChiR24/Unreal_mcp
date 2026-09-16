#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersKeyResolution.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersMappingSummaries.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
void AddInputMappingSummary(
    TSharedPtr<FJsonObject> Result,
    const UInputMappingContext* Context,
    const UInputAction* InAction)
{
    TArray<TSharedPtr<FJsonValue>> Mappings;
    for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
    {
        if (Mapping.Action != InAction)
        {
            continue;
        }

        TSharedPtr<FJsonObject> MappingObject = MakeShared<FJsonObject>();
        MappingObject->SetStringField(TEXT("key"), Mapping.Key.ToString());
        MappingObject->SetNumberField(TEXT("modifierCount"), Mapping.Modifiers.Num());
        MappingObject->SetNumberField(TEXT("triggerCount"), Mapping.Triggers.Num());
        Mappings.Add(MakeShared<FJsonValueObject>(MappingObject));
    }

    Result->SetNumberField(TEXT("mappingCount"), Mappings.Num());
    Result->SetArrayField(TEXT("mappings"), Mappings);
}

bool HandleAddInputMapping(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    FString SanitizedContextPath;
    FString SanitizedActionPath;
    UInputMappingContext* Context = LoadInputMappingContextAsset(ContextPath, SanitizedContextPath);
    UInputAction* InAction = LoadInputActionAsset(ActionPath, SanitizedActionPath);

    if (!Context || !InAction || KeyName.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Context or action not found, or key is empty. Context: %s, Action: %s"),
                *SanitizedContextPath, *SanitizedActionPath),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FKey Key = FKey(FName(*KeyName));
    if (!Key.IsValid())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Invalid key name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // triggerType and modifierType are declared on this action and its own
    // whenToUse promises them ("with optional trigger and modifier types"), but
    // only MapKey was called -- both were accepted and dropped. They belong on
    // the mapping's arrays, which are separate objects from the action's own.
    // MapKey ALWAYS appends. Re-adding the same action+key therefore produced a
    // SECOND mapping that Enhanced Input evaluates independently: the key fired
    // its action twice, and a modifier set the caller believed they were
    // correcting stayed live on the first copy. Naming the pair again means that
    // mapping, so reuse it and let this call define its triggers and modifiers.
    FEnhancedActionKeyMapping* ExistingMapping = nullptr;
    const int32 ExistingCount = Context->GetMappings().Num();
    for (int32 MappingIndex = 0; MappingIndex < ExistingCount; ++MappingIndex)
    {
        FEnhancedActionKeyMapping& Candidate = Context->GetMapping(MappingIndex);
        if (Candidate.Action == InAction && Candidate.Key == Key)
        {
            ExistingMapping = &Candidate;
            break;
        }
    }
    const bool bReusedExistingMapping = ExistingMapping != nullptr;
    FEnhancedActionKeyMapping& Mapping =
        bReusedExistingMapping ? *ExistingMapping : Context->MapKey(InAction, Key);
    if (bReusedExistingMapping)
    {
        Mapping.Triggers.Reset();
        Mapping.Modifiers.Reset();
    }
    FString TriggerType;
    Payload->TryGetStringField(TEXT("triggerType"), TriggerType);
    FString ModifierType;
    Payload->TryGetStringField(TEXT("modifierType"), ModifierType);
    TArray<FString> Unresolved;
    if (!TriggerType.IsEmpty())
    {
        const FString ClassName = TriggerType.StartsWith(TEXT("InputTrigger"))
            ? TriggerType : TEXT("InputTrigger") + TriggerType;
        UClass* TriggerClass = ResolveClassByName(ClassName);
        if (TriggerClass && TriggerClass->IsChildOf(UInputTrigger::StaticClass()))
        {
            Mapping.Triggers.Add(NewObject<UInputTrigger>(Context, TriggerClass));
        }
        else { Unresolved.Add(FString::Printf(TEXT("triggerType '%s'"), *TriggerType)); }
    }
    if (!ModifierType.IsEmpty())
    {
        const FString ClassName = ModifierType.StartsWith(TEXT("InputModifier"))
            ? ModifierType : TEXT("InputModifier") + ModifierType;
        UClass* ModifierClass = ResolveClassByName(ClassName);
        if (ModifierClass && ModifierClass->IsChildOf(UInputModifier::StaticClass()))
        {
            Mapping.Modifiers.Add(NewObject<UInputModifier>(Context, ModifierClass));
        }
        else { Unresolved.Add(FString::Printf(TEXT("modifierType '%s'"), *ModifierType)); }
    }
    if (Unresolved.Num() > 0)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not resolve %s to an Enhanced Input class; the key mapping was not added."),
                *FString::Join(Unresolved, TEXT(" and "))),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    Context->Modify();
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("key"), KeyName);
    Result->SetNumberField(TEXT("triggerCount"), Mapping.Triggers.Num());
    Result->SetNumberField(TEXT("modifierCount"), Mapping.Modifiers.Num());
    Result->SetBoolField(TEXT("reusedExistingMapping"), bReusedExistingMapping);
    AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
    AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        SubAction == TEXT("map_input_action") ?
        TEXT("Input action mapped to key.") : TEXT("Mapping added."), Result);
    return true;
}

bool HandleRemoveInputMapping(
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

    FString SanitizedContextPath;
    FString SanitizedActionPath;
    UInputMappingContext* Context = LoadInputMappingContextAsset(ContextPath, SanitizedContextPath);
    UInputAction* InAction = LoadInputActionAsset(ActionPath, SanitizedActionPath);

    if (!Context || !InAction)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Context or action not found. Context: %s, Action: %s"),
                *SanitizedContextPath, *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    FKey RequestedKey = InputKeyFromName(KeyName);
    const bool bHasSpecificKey = !KeyName.IsEmpty();
    if (bHasSpecificKey && !RequestedKey.IsValid())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid key name: %s"), *KeyName), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<FKey> KeysToRemove;
    for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
    {
        if (Mapping.Action == InAction && (!bHasSpecificKey || Mapping.Key == RequestedKey))
        {
            KeysToRemove.Add(Mapping.Key);
        }
    }

    if (bHasSpecificKey && KeysToRemove.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Mapping not found for action '%s' and key '%s'."),
                *SanitizedActionPath, *KeyName),
            TEXT("NOT_FOUND"));
        return true;
    }

    for (const FKey& KeyToRemove : KeysToRemove)
    {
        Context->UnmapKey(InAction, KeyToRemove);
    }

    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    if (bHasSpecificKey)
    {
        Result->SetStringField(TEXT("key"), KeyName);
    }
    Result->SetNumberField(TEXT("keysRemoved"), KeysToRemove.Num());

    TArray<TSharedPtr<FJsonValue>> RemovedKeys;
    for (const FKey& Key : KeysToRemove)
    {
        RemovedKeys.Add(MakeShared<FJsonValueString>(Key.ToString()));
    }
    Result->SetArrayField(TEXT("removedKeys"), RemovedKeys);
    AddInputMappingSummary(Result, Context, InAction);
    AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
    AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

    const FString SuccessMessage = bHasSpecificKey
        ? FString::Printf(TEXT("Mapping removed for action key: %s"), *KeyName)
        : TEXT("Mappings removed for action.");
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, SuccessMessage, Result);
    return true;
}
#endif
}
