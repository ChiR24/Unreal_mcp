#include "Domains/AI/McpAIContext.h"
#include "Domains/AI/Blackboard/McpAIBlackboardDefaults.h"

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
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

namespace
{
// keyType as the Blackboard editor names the types; an unknown one used to become an Object key.
UClass* BlackboardKeyClass(const FString& KeyType)
{
    const TPair<const TCHAR*, UClass*> Types[] = {
        {TEXT("Bool"), UBlackboardKeyType_Bool::StaticClass()}, {TEXT("Int"), UBlackboardKeyType_Int::StaticClass()},
        {TEXT("Float"), UBlackboardKeyType_Float::StaticClass()}, {TEXT("Vector"), UBlackboardKeyType_Vector::StaticClass()},
        {TEXT("Rotator"), UBlackboardKeyType_Rotator::StaticClass()}, {TEXT("Object"), UBlackboardKeyType_Object::StaticClass()},
        {TEXT("Class"), UBlackboardKeyType_Class::StaticClass()}, {TEXT("Enum"), UBlackboardKeyType_Enum::StaticClass()},
        {TEXT("Name"), UBlackboardKeyType_Name::StaticClass()}, {TEXT("String"), UBlackboardKeyType_String::StaticClass()}};
    for (const TPair<const TCHAR*, UClass*>& Type : Types)
    {
        if (KeyType.Equals(Type.Key, ESearchCase::IgnoreCase))
        {
            return Type.Value;
        }
    }
    return nullptr;
}
}

namespace McpAIHandlers
{
// Implements the "add_blackboard_key" action.
bool HandleAddBlackboardKey(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const auto Fail = [&](const FString& Message, const TCHAR* FailCode)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Message, FailCode);
        return true;
    };
    FString Error;
    FString Code;
    UBlackboardData* Blackboard = LoadEditableBlackboard(Payload, Error, Code);
    if (!Blackboard)
    {
        return Fail(Error, *Code);
    }
    const FString KeyName = GetJsonStringField(Payload, TEXT("keyName")).TrimStartAndEnd();
    UClass* KeyClass = BlackboardKeyClass(GetJsonStringField(Payload, TEXT("keyType")));
    const FString BaseClassName = GetJsonStringField(Payload, TEXT("baseObjectClass"));
    const FString EnumPath = GetJsonStringField(Payload, TEXT("enumType"));
    const bool bTakesBaseClass = KeyClass == UBlackboardKeyType_Object::StaticClass() || KeyClass == UBlackboardKeyType_Class::StaticClass();
    UClass* BaseClass = BaseClassName.IsEmpty() ? nullptr : ResolveClassByName(BaseClassName);
    UEnum* EnumType = EnumPath.IsEmpty() ? nullptr : LoadObject<UEnum>(nullptr, *EnumPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!EnumType && !EnumPath.IsEmpty())
    {
        EnumType = Cast<UEnum>(McpLoadAsset(EnumPath));
    }

    if (KeyName.IsEmpty())
    {
        return Fail(TEXT("keyName is required"), TEXT("INVALID_ARGUMENT"));
    }
    // GetKeyID looks through the parent chain too: a second key of one name was appended before and shadowed the first.
    if (Blackboard->GetKeyID(FName(*KeyName)) != FBlackboard::InvalidKey)
    {
        return Fail(FString::Printf(TEXT("%s already has a key named %s (its own or its parent's); pick another name. Nothing was added."),
            *Blackboard->GetPathName(), *KeyName), TEXT("NAME_TAKEN"));
    }
    if (!KeyClass)
    {
        return Fail(TEXT("keyType must be Bool, Int, Float, Vector, Rotator, Object, Class, Enum, Name or String."), TEXT("INVALID_ARGUMENT"));
    }
    if (!BaseClassName.IsEmpty() && !bTakesBaseClass)
    {
        return Fail(TEXT("baseObjectClass applies to Object and Class keys only."), TEXT("INVALID_ARGUMENT"));
    }
    if (!BaseClassName.IsEmpty() && !BaseClass)
    {
        return Fail(FString::Printf(TEXT("Class not found: %s"), *BaseClassName), TEXT("CLASS_NOT_FOUND"));
    }
    if (!EnumPath.IsEmpty() && KeyClass != UBlackboardKeyType_Enum::StaticClass())
    {
        return Fail(TEXT("enumType applies to Enum keys only."), TEXT("INVALID_ARGUMENT"));
    }
    if (!EnumPath.IsEmpty() && !EnumType)
    {
        return Fail(FString::Printf(TEXT("Enum not found: %s; pass its path, e.g. /Game/Enums/E_State or /Script/Engine.ECollisionChannel."), *EnumPath), TEXT("NOT_FOUND"));
    }

    Blackboard->Modify();
    FBlackboardEntry Entry;
    Entry.EntryName = FName(*KeyName);
    Entry.KeyType = NewObject<UBlackboardKeyType>(Blackboard, KeyClass);
    Entry.bInstanceSynced = GetJsonBoolField(Payload, TEXT("isInstanceSynced"), false);
    UClass* KeptBaseClass = nullptr;
    // An Object key keeps the engine's base class Object unless told otherwise; actor filters such as MoveTo's skip it.
    if (UBlackboardKeyType_Object* ObjectKey = Cast<UBlackboardKeyType_Object>(Entry.KeyType))
    {
        if (BaseClass)
        {
            ObjectKey->BaseClass = BaseClass;
        }
        KeptBaseClass = ObjectKey->BaseClass.Get();
    }
    else if (UBlackboardKeyType_Class* ClassKey = Cast<UBlackboardKeyType_Class>(Entry.KeyType))
    {
        if (BaseClass)
        {
            ClassKey->BaseClass = BaseClass;
        }
        KeptBaseClass = ClassKey->BaseClass.Get();
    }
    else if (UBlackboardKeyType_Enum* EnumKey = Cast<UBlackboardKeyType_Enum>(Entry.KeyType))
    {
        EnumKey->EnumType = EnumType;
    }
    const int32 KeyIndex = Blackboard->Keys.Add(Entry);
    NotifyBlackboardKeysChanged(Blackboard);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("keyIndex"), KeyIndex);
    Result->SetStringField(TEXT("keyName"), KeyName);
    Result->SetStringField(TEXT("keyType"), KeyClass->GetName().Replace(TEXT("BlackboardKeyType_"), TEXT("")));
    if (KeptBaseClass)
    {
        Result->SetStringField(TEXT("baseClass"), KeptBaseClass->GetPathName());
    }
    if (EnumType)
    {
        Result->SetStringField(TEXT("enumType"), EnumType->GetPathName());
    }
    Result->SetBoolField(TEXT("isInstanceSynced"), Entry.bInstanceSynced);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blackboard));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Blackboard key %s added"), *KeyName), Result);
    return true;
}

// Implements the "set_key_instance_synced" action.
bool HandleSetKeyInstanceSynced(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Error;
    FString Code;
    UBlackboardData* Blackboard = LoadEditableBlackboard(Payload, Error, Code);
    FBlackboardEntry* Key = Blackboard ? FindOwnBlackboardKey(Blackboard, GetJsonStringField(Payload, TEXT("keyName")), Error, Code) : nullptr;
    if (!Key)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }

    const bool bSynced = GetJsonBoolField(Payload, TEXT("isInstanceSynced"), true);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("keyName"), Key->EntryName.ToString());
    Result->SetBoolField(TEXT("isInstanceSynced"), bSynced);
    Result->SetBoolField(TEXT("changed"), Key->bInstanceSynced != bSynced);
    if (Key->bInstanceSynced == bSynced)
    {
        McpHandlerUtils::MarkNoAssetsChanged(Result);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("The key already had that setting; nothing changed"), Result);
        return true;
    }

    Blackboard->Modify();
    Key->bInstanceSynced = bSynced;
    NotifyBlackboardKeysChanged(Blackboard);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blackboard));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Key instance sync updated"), Result);
    return true;
}
}
