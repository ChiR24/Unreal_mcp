#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BlackboardData.h"
#include "EditorAssetLibrary.h"

namespace McpAIHandlers
{
bool HandleAddBlackboardKey(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_blackboard_key");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_blackboard_key"))
    {
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        FString KeyType = GetStringFieldAI(Payload, TEXT("keyType"));

        // CRITICAL: Explicitly check if asset exists before LoadObject
        if (!UEditorAssetLibrary::DoesAssetExist(BlackboardPath))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!Blackboard)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Create appropriate key type
        FBlackboardEntry NewEntry;
        NewEntry.EntryName = FName(*KeyName);

        if (KeyType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Bool>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Int>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Float>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Rotator>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
        {
            UBlackboardKeyType_Object* ObjectKey = NewObject<UBlackboardKeyType_Object>(Blackboard);
            FString BaseClass = GetStringFieldAI(Payload, TEXT("baseObjectClass"), TEXT("Actor"));
            // Could set base class here
            NewEntry.KeyType = ObjectKey;
        }
        else if (KeyType.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Class>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Enum"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Enum>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Name>(Blackboard);
        }
        else if (KeyType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
        {
            NewEntry.KeyType = NewObject<UBlackboardKeyType_String>(Blackboard);
        }
        else
        {
            // Default to Object
            NewEntry.KeyType = NewObject<UBlackboardKeyType_Object>(Blackboard);
        }

        NewEntry.bInstanceSynced = GetBoolFieldAI(Payload, TEXT("isInstanceSynced"), false);

        Blackboard->Keys.Add(NewEntry);
        Blackboard->MarkPackageDirty();
        McpSafeAssetSave(Blackboard);

        Result->SetNumberField(TEXT("keyIndex"), Blackboard->Keys.Num() - 1);
        Result->SetStringField(TEXT("keyName"), KeyName);
        Result->SetStringField(TEXT("keyType"), KeyType);
        McpHandlerUtils::AddVerification(Result, Blackboard);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard key added"), Result);
        return true;
    }

    return true;
}

bool HandleSetKeyInstanceSynced(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_key_instance_synced");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("set_key_instance_synced"))
    {
        FString BlackboardPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
        FString KeyName = GetStringFieldAI(Payload, TEXT("keyName"));
        bool bInstanceSynced = GetBoolFieldAI(Payload, TEXT("isInstanceSynced"), true);

        UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!Blackboard)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        bool bFound = false;
        for (FBlackboardEntry& Entry : Blackboard->Keys)
        {
            if (Entry.EntryName.ToString() == KeyName)
            {
                Entry.bInstanceSynced = bInstanceSynced;
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Key not found: %s"), *KeyName),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Blackboard->MarkPackageDirty();
        McpSafeAssetSave(Blackboard);

        Result->SetStringField(TEXT("keyName"), KeyName);
        Result->SetBoolField(TEXT("isInstanceSynced"), bInstanceSynced);
        McpHandlerUtils::AddVerification(Result, Blackboard);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Key instance sync updated"), Result);
        return true;
    }

    // =========================================================================
    // 16.3 Behavior Tree - Expanded (6 actions)
    // =========================================================================

    return true;
}
}
#endif
