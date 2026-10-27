#include "Domains/AI/McpAIContext.h"
#include "Domains/AI/Blackboard/McpAIBlackboardDefaults.h"

#include "BehaviorTree/BlackboardData.h"

namespace McpAIHandlers
{
// Implements the "set_blackboard_value" action: the key's default, which every Blackboard made from the asset starts with.
// A type without one, an empty value or a value of the wrong kind used to answer success with valueSet false, saved anyway.
bool HandleSetBlackboardValue(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Error;
    FString Code;
    UBlackboardData* Blackboard = LoadEditableBlackboard(Payload, Error, Code);
    FBlackboardEntry* Key = Blackboard ? FindOwnBlackboardKey(Blackboard, GetJsonStringField(Payload, TEXT("keyName")), Error, Code) : nullptr;
    const TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"));
    if (Key && !Key->KeyType)
    {
        Error = FString::Printf(TEXT("Key '%s' has no type to hold a value."), *Key->EntryName.ToString());
        Code = TEXT("INVALID_TARGET");
        Key = nullptr;
    }
    else if (Key && (!Value.IsValid() || Value->IsNull()))
    {
        Error = TEXT("value is required: the default the key starts with in every Blackboard made from this asset.");
        Code = TEXT("INVALID_ARGUMENT");
        Key = nullptr;
    }
    if (!Key || !WriteBlackboardKeyDefault(Key->KeyType, Value, Error, Code))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }
    NotifyBlackboardKeysChanged(Blackboard);

    FString Stored;
    ReadBlackboardKeyDefault(Key->KeyType, Stored);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blackboardPath"), Blackboard->GetPathName());
    Result->SetStringField(TEXT("keyName"), Key->EntryName.ToString());
    Result->SetStringField(TEXT("keyType"), Key->KeyType->GetClass()->GetName().Replace(TEXT("BlackboardKeyType_"), TEXT("")));
    Result->SetStringField(TEXT("value"), Stored);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blackboard));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard key default set"), Result);
    return true;
}

// Implements the "get_blackboard_value" action.
bool HandleGetBlackboardValue(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString BBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    UBlackboardData* BBData = BBPath.IsEmpty() ? nullptr : LoadObject<UBlackboardData>(nullptr, *BBPath);
    FString Error = BBPath.IsEmpty() ? FString(TEXT("blackboardPath is required")) : FString::Printf(TEXT("Blackboard not found: %s"), *BBPath);
    FString Code = BBPath.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("NOT_FOUND");
    const FBlackboardEntry* Key = BBData ? FindOwnBlackboardKey(BBData, GetJsonStringField(Payload, TEXT("keyName")), Error, Code) : nullptr;
    if (!Key)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }

    const FString KeyType = Key->KeyType ? Key->KeyType->GetClass()->GetName() : TEXT("Unknown");
    FString ValueStr;
    const bool bValueAvailable = Key->KeyType && ReadBlackboardKeyDefault(Key->KeyType, ValueStr);
    TSharedPtr<FJsonObject> GetResult = McpHandlerUtils::CreateResultObject();
    GetResult->SetStringField(TEXT("blackboardPath"), BBPath);
    GetResult->SetStringField(TEXT("keyName"), Key->EntryName.ToString());
    GetResult->SetStringField(TEXT("keyType"), KeyType);
    GetResult->SetBoolField(TEXT("instanceSynced"), Key->bInstanceSynced);
    GetResult->SetBoolField(TEXT("valueAvailable"), bValueAvailable);
    if (bValueAvailable)
    {
        GetResult->SetStringField(TEXT("value"), ValueStr);
        // Typed companions so callers need not parse the text (dogfood #65).
        if (KeyType.EndsWith(TEXT("_Float")) || KeyType.EndsWith(TEXT("_Int")))
        {
            GetResult->SetNumberField(TEXT("valueNumber"), FCString::Atod(*ValueStr));
        }
        else if (KeyType.EndsWith(TEXT("_Bool")))
        {
            GetResult->SetBoolField(TEXT("valueBool"), ValueStr.ToBool());
        }
    }
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard value retrieved"), GetResult);
    return true;
}
}
