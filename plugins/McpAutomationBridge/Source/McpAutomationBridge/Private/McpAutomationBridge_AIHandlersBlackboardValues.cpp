#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BlackboardData.h"

namespace McpAIHandlers
{
bool HandleSetBlackboardValue(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_blackboard_value");
    if (SubAction == TEXT("set_blackboard_value"))
    {
        FString BBPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        if (BBPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blackboardPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        if (KeyName.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing keyName"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlackboardData* BBData = LoadObject<UBlackboardData>(nullptr, *BBPath);
        if (!BBData)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BBPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the key and set its value
        bool bKeyFound = false;
        bool bValueSet = false;
        FString ValueStr = GetStringFieldAI(Payload, TEXT("value"));

        for (FBlackboardEntry& Key : BBData->Keys)
        {
            if (Key.EntryName.ToString() == KeyName)
            {
                bKeyFound = true;

                // Set the default value based on key type
                // Note: DefaultValue properties on BlackboardKeyType are only available in UE 5.5+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
                if (Key.KeyType && !ValueStr.IsEmpty())
                {
                    if (UBlackboardKeyType_Bool* BoolKey = Cast<UBlackboardKeyType_Bool>(Key.KeyType))
                    {
                        BoolKey->bDefaultValue = ValueStr.ToLower() == TEXT("true") || ValueStr == TEXT("1");
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_Int* IntKey = Cast<UBlackboardKeyType_Int>(Key.KeyType))
                    {
                        IntKey->DefaultValue = FCString::Atoi(*ValueStr);
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_Float* FloatKey = Cast<UBlackboardKeyType_Float>(Key.KeyType))
                    {
                        FloatKey->DefaultValue = FCString::Atof(*ValueStr);
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_Vector* VectorKey = Cast<UBlackboardKeyType_Vector>(Key.KeyType))
                    {
                        VectorKey->DefaultValue.InitFromString(ValueStr);
                        VectorKey->bUseDefaultValue = true;
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_Rotator* RotatorKey = Cast<UBlackboardKeyType_Rotator>(Key.KeyType))
                    {
                        RotatorKey->DefaultValue.InitFromString(ValueStr);
                        RotatorKey->bUseDefaultValue = true;
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_Name* NameKey = Cast<UBlackboardKeyType_Name>(Key.KeyType))
                    {
                        NameKey->DefaultValue = FName(*ValueStr);
                        bValueSet = true;
                    }
                    else if (UBlackboardKeyType_String* StringKey = Cast<UBlackboardKeyType_String>(Key.KeyType))
                    {
                        StringKey->DefaultValue = ValueStr;
                        bValueSet = true;
                    }
                    else
                    {
                        // Unsupported key type - note in response
                        bValueSet = false;
                    }
                }
#else
                // UE 5.0-5.4: DefaultValue properties not available on BlackboardKeyType
                // Value setting requires UE 5.5+
                bValueSet = false;
#endif
                break;
            }
        }

        if (!bKeyFound)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Key '%s' not found in blackboard"), *KeyName), TEXT("KEY_NOT_FOUND"));
            return true;
        }

        McpSafeAssetSave(BBData);

        TSharedPtr<FJsonObject> SetResult = McpHandlerUtils::CreateResultObject();
        SetResult->SetStringField(TEXT("blackboardPath"), BBPath);
        SetResult->SetStringField(TEXT("keyName"), KeyName);
        SetResult->SetStringField(TEXT("value"), ValueStr);
        SetResult->SetBoolField(TEXT("valueSet"), bValueSet);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
            bValueSet ? TEXT("Blackboard value set") : TEXT("Key found but value not set (unsupported type)"), SetResult);
#else
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Key found. Note: set_blackboard_value requires UE 5.5+ for value setting."), SetResult);
#endif
        return true;
    }

    // get_blackboard_value - Get a key's info from a blackboard asset
    return true;
}

bool HandleGetBlackboardValue(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("get_blackboard_value");
    if (SubAction == TEXT("get_blackboard_value"))
    {
        FString BBPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        if (BBPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blackboardPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        if (KeyName.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing keyName"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlackboardData* BBData = LoadObject<UBlackboardData>(nullptr, *BBPath);
        if (!BBData)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BBPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the key
        bool bKeyFound = false;
        FString KeyType = TEXT("Unknown");
        bool bInstanceSynced = false;

        for (const FBlackboardEntry& Key : BBData->Keys)
        {
            if (Key.EntryName.ToString() == KeyName)
            {
                bKeyFound = true;
                bInstanceSynced = Key.bInstanceSynced;
                if (Key.KeyType)
                {
                    KeyType = Key.KeyType->GetClass()->GetName();
                }
                break;
            }
        }

        if (!bKeyFound)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Key '%s' not found in blackboard"), *KeyName), TEXT("KEY_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> GetResult = McpHandlerUtils::CreateResultObject();
        GetResult->SetStringField(TEXT("blackboardPath"), BBPath);
        GetResult->SetStringField(TEXT("keyName"), KeyName);
        GetResult->SetStringField(TEXT("keyType"), KeyType);
        GetResult->SetBoolField(TEXT("instanceSynced"), bInstanceSynced);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard value retrieved"), GetResult);
        return true;
    }

    // run_behavior_tree - Alias for assign_behavior_tree
    return true;
}
}
#endif
