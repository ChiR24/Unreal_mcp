#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InputHandlersAssetResolution.h"
#include "McpAutomationBridge_InputHandlersKeyResolution.h"
#include "McpAutomationBridge_InputHandlersMappingSummaries.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

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

    Context->MapKey(InAction, Key);
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("key"), KeyName);
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
