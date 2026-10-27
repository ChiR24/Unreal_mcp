#include "Domains/AI/Blackboard/McpAIBlackboardDefaults.h"
#include "Domains/AI/McpAIContext.h"

#include "BehaviorTree/BlackboardData.h"
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
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Misc/DefaultValueHelper.h"

namespace
{
bool Refuse(FString& OutError, FString& OutCode, const FString& Error, const TCHAR* Code = TEXT("INVALID_ARGUMENT"))
{
    OutError = Error;
    OutCode = Code;
    return false;
}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
// true/false, 1/0, or the same as text.
bool ReadBoolValue(const TSharedPtr<FJsonValue>& Value, bool& Out)
{
    FString Text;
    if (!McpJsonScalarToString(Value, Text))
    {
        return false;
    }
    Out = Text.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Text == TEXT("1");
    return Out || Text.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Text == TEXT("0");
}

// {x, y, z} or [x, y, z], else the engine's text (X=1 Y=2 Z=3) or 1,2,3.
bool ReadVectorValue(const TSharedPtr<FJsonValue>& Value, FVector& Out)
{
    static const TCHAR* const Keys[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
    double V[3] = {0.0, 0.0, 0.0};
    FString Text;
    if (ReadJsonTriple(Value, Keys, V))
    {
        Out = FVector(V[0], V[1], V[2]);
        return true;
    }
    return McpJsonScalarToString(Value, Text) && (Out.InitFromString(Text) || FDefaultValueHelper::ParseVector(Text, Out));
}

// {pitch, yaw, roll} or [pitch, yaw, roll], else the engine's text (P=0 Y=90 R=0) or 0,90,0.
bool ReadRotatorValue(const TSharedPtr<FJsonValue>& Value, FRotator& Out)
{
    static const TCHAR* const Keys[3] = {TEXT("pitch"), TEXT("yaw"), TEXT("roll")};
    double V[3] = {0.0, 0.0, 0.0};
    FString Text;
    if (ReadJsonTriple(Value, Keys, V))
    {
        Out = FRotator(V[0], V[1], V[2]);
        return true;
    }
    return McpJsonScalarToString(Value, Text) && (Out.InitFromString(Text) || FDefaultValueHelper::ParseRotator(Text, Out));
}
#endif

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
// An enumerator by name, display name or value; the key stores it in a byte.
bool ReadEnumValue(const UEnum* Enum, const TSharedPtr<FJsonValue>& Value, uint8& Out)
{
    FString Text;
    int32 Number = INDEX_NONE;
    if (!McpJsonScalarToString(Value, Text))
    {
        return false;
    }
    for (int32 Index = 0; Index < Enum->NumEnums() && Number == INDEX_NONE; ++Index)
    {
        if (Enum->GetNameStringByIndex(Index).Equals(Text, ESearchCase::IgnoreCase) ||
            Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(Text, ESearchCase::IgnoreCase))
        {
            Number = static_cast<int32>(Enum->GetValueByIndex(Index));
        }
    }
    if (Number == INDEX_NONE && !(FDefaultValueHelper::ParseInt(Text, Number) && Enum->IsValidEnumValue(Number)))
    {
        return false;
    }
    Out = static_cast<uint8>(Number);
    return Number >= 0 && Number <= MAX_uint8;
}
#endif
}

namespace McpAIHandlers
{
UBlackboardData* LoadEditableBlackboard(const TSharedPtr<FJsonObject>& Payload, FString& OutError, FString& OutCode)
{
    const FString Path = GetJsonStringField(Payload, TEXT("blackboardPath"));
    UBlackboardData* Blackboard = Path.IsEmpty() ? nullptr : LoadObject<UBlackboardData>(nullptr, *Path);
    if (!Blackboard)
    {
        Refuse(OutError, OutCode, Path.IsEmpty() ? FString(TEXT("blackboardPath is required"))
            : FString::Printf(TEXT("Blackboard not found: %s"), *Path), Path.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("NOT_FOUND"));
        return nullptr;
    }
    if (Blackboard->GetOutermost()->GetName().StartsWith(TEXT("/Engine/")))
    {
        Refuse(OutError, OutCode, TEXT("Engine content is not changed: duplicate the Blackboard into /Game and edit the copy."), TEXT("ENGINE_CONTENT"));
        return nullptr;
    }
    return Blackboard;
}

FBlackboardEntry* FindOwnBlackboardKey(UBlackboardData* Blackboard, const FString& KeyName, FString& OutError, FString& OutCode)
{
    TArray<FString> Names;
    for (FBlackboardEntry& Entry : Blackboard->Keys)
    {
        if (Entry.EntryName.ToString().Equals(KeyName, ESearchCase::IgnoreCase))
        {
            return &Entry;
        }
        Names.Add(Entry.EntryName.ToString());
    }
    if (KeyName.IsEmpty())
    {
        Refuse(OutError, OutCode, TEXT("keyName is required"));
    }
    else if (Blackboard->Parent && Blackboard->Parent->GetKeyID(FName(*KeyName)) != FBlackboard::InvalidKey)
    {
        Refuse(OutError, OutCode, FString::Printf(TEXT("Key '%s' comes from the parent Blackboard %s; change it there."),
            *KeyName, *Blackboard->Parent->GetPathName()), TEXT("KEY_IN_PARENT"));
    }
    else
    {
        Refuse(OutError, OutCode, FString::Printf(TEXT("Key '%s' is not in %s; its keys: %s."), *KeyName, *Blackboard->GetPathName(),
            Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("none")), TEXT("KEY_NOT_FOUND"));
    }
    return nullptr;
}

void NotifyBlackboardKeysChanged(UBlackboardData* Blackboard)
{
    Blackboard->UpdateIfHasSynchronizedKeys();
    FPropertyChangedEvent Event(FindFProperty<FProperty>(UBlackboardData::StaticClass(), GET_MEMBER_NAME_CHECKED(UBlackboardData, Keys)));
    Blackboard->PostEditChangeProperty(Event);
    Blackboard->MarkPackageDirty();
}

bool WriteBlackboardKeyDefault(UBlackboardKeyType* KeyType, const TSharedPtr<FJsonValue>& Value, FString& OutError, FString& OutCode)
{
    const FString Type = KeyType->GetClass()->GetName().Replace(TEXT("BlackboardKeyType_"), TEXT(""));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
    const FString Bad = FString::Printf(TEXT("value does not fit a %s key: "), *Type);
    FString Text;
    if (UBlackboardKeyType_Bool* Key = Cast<UBlackboardKeyType_Bool>(KeyType))
    {
        bool Parsed = false;
        if (!ReadBoolValue(Value, Parsed)) { return Refuse(OutError, OutCode, Bad + TEXT("send true or false.")); }
        Key->Modify();
        Key->bDefaultValue = Parsed;
        return true;
    }
    if (UBlackboardKeyType_Int* Key = Cast<UBlackboardKeyType_Int>(KeyType))
    {
        int32 Parsed = 0;
        if (!McpJsonScalarToString(Value, Text) || !FDefaultValueHelper::ParseInt(Text, Parsed)) { return Refuse(OutError, OutCode, Bad + TEXT("send a whole number.")); }
        Key->Modify();
        Key->DefaultValue = Parsed;
        return true;
    }
    if (UBlackboardKeyType_Float* Key = Cast<UBlackboardKeyType_Float>(KeyType))
    {
        float Parsed = 0.0f;
        if (!McpJsonScalarToString(Value, Text) || !FDefaultValueHelper::ParseFloat(Text, Parsed)) { return Refuse(OutError, OutCode, Bad + TEXT("send a number.")); }
        Key->Modify();
        Key->DefaultValue = Parsed;
        return true;
    }
    if (UBlackboardKeyType_Vector* Key = Cast<UBlackboardKeyType_Vector>(KeyType))
    {
        FVector Parsed = FVector::ZeroVector;
        if (!ReadVectorValue(Value, Parsed)) { return Refuse(OutError, OutCode, Bad + TEXT("send {x, y, z}.")); }
        Key->Modify();
        Key->DefaultValue = Parsed;
        Key->bUseDefaultValue = true;
        return true;
    }
    if (UBlackboardKeyType_Rotator* Key = Cast<UBlackboardKeyType_Rotator>(KeyType))
    {
        FRotator Parsed = FRotator::ZeroRotator;
        if (!ReadRotatorValue(Value, Parsed)) { return Refuse(OutError, OutCode, Bad + TEXT("send {pitch, yaw, roll}.")); }
        Key->Modify();
        Key->DefaultValue = Parsed;
        Key->bUseDefaultValue = true;
        return true;
    }
    UBlackboardKeyType_Name* NameKey = Cast<UBlackboardKeyType_Name>(KeyType);
    UBlackboardKeyType_String* StringKey = Cast<UBlackboardKeyType_String>(KeyType);
    if (NameKey || StringKey)
    {
        if (!McpJsonScalarToString(Value, Text)) { return Refuse(OutError, OutCode, Bad + TEXT("send text.")); }
        KeyType->Modify();
        if (NameKey) { NameKey->DefaultValue = FName(*Text); } else { StringKey->DefaultValue = Text; }
        return true;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    if (UBlackboardKeyType_Enum* Key = Cast<UBlackboardKeyType_Enum>(KeyType))
    {
        uint8 Parsed = 0;
        if (!Key->EnumType) { return Refuse(OutError, OutCode, TEXT("The key has no enum type to take a value from; add it again with enumType."), TEXT("NOT_SUPPORTED")); }
        if (!ReadEnumValue(Key->EnumType, Value, Parsed)) { return Refuse(OutError, OutCode, Bad + FString::Printf(TEXT("send an enumerator of %s by name or value."), *Key->EnumType->GetName())); }
        Key->Modify();
        Key->DefaultValue = Parsed;
        return true;
    }
    UBlackboardKeyType_Object* ObjectKey = Cast<UBlackboardKeyType_Object>(KeyType);
    UBlackboardKeyType_Class* ClassKey = Cast<UBlackboardKeyType_Class>(KeyType);
    if (ObjectKey || ClassKey)
    {
        // An asset (Object) or a class (Class) path; None clears it. Placed actors are not assets and cannot be defaults.
        if (!McpJsonScalarToString(Value, Text)) { return Refuse(OutError, OutCode, Bad + TEXT("send an asset or class path, or None.")); }
        const bool bClear = Text.IsEmpty() || Text == TEXT("None");
        UObject* Object = bClear || ClassKey ? nullptr : McpLoadAsset(Text);
        UClass* Class = bClear || ObjectKey ? nullptr : ResolveClassByName(Text);
        UClass* BaseClass = ObjectKey ? ObjectKey->BaseClass.Get() : ClassKey->BaseClass.Get();
        if (!bClear && !Object && !Class) { return Refuse(OutError, OutCode, FString::Printf(TEXT("%s not found: %s"), ObjectKey ? TEXT("Asset") : TEXT("Class"), *Text), TEXT("NOT_FOUND")); }
        if (BaseClass && ((Object && !Object->IsA(BaseClass)) || (Class && !Class->IsChildOf(BaseClass))))
        {
            return Refuse(OutError, OutCode, FString::Printf(TEXT("%s does not derive from %s, the base class of the key."), *Text, *BaseClass->GetName()), TEXT("TYPE_MISMATCH"));
        }
        KeyType->Modify();
        if (ObjectKey) { ObjectKey->DefaultValue = Object; } else { ClassKey->DefaultValue = Class; }
        return true;
    }
#endif
    return Refuse(OutError, OutCode, FString::Printf(TEXT("A %s key has no default value to set on this engine version."), *Type), TEXT("NOT_SUPPORTED"));
#else
    return Refuse(OutError, OutCode, FString::Printf(TEXT("Blackboard key defaults exist from UE 5.4; this engine keeps none for the %s key."), *Type), TEXT("NOT_SUPPORTED"));
#endif
}

bool ReadBlackboardKeyDefault(const UBlackboardKeyType* KeyType, FString& OutText)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
    if (const UBlackboardKeyType_Bool* Bool = Cast<UBlackboardKeyType_Bool>(KeyType)) { OutText = Bool->bDefaultValue ? TEXT("true") : TEXT("false"); return true; }
    if (const UBlackboardKeyType_Int* Int = Cast<UBlackboardKeyType_Int>(KeyType)) { OutText = FString::FromInt(Int->DefaultValue); return true; }
    if (const UBlackboardKeyType_Float* Float = Cast<UBlackboardKeyType_Float>(KeyType)) { OutText = FString::SanitizeFloat(Float->DefaultValue); return true; }
    if (const UBlackboardKeyType_Vector* Vector = Cast<UBlackboardKeyType_Vector>(KeyType)) { OutText = Vector->DefaultValue.ToString(); return Vector->bUseDefaultValue; }
    if (const UBlackboardKeyType_Rotator* Rotator = Cast<UBlackboardKeyType_Rotator>(KeyType)) { OutText = Rotator->DefaultValue.ToString(); return Rotator->bUseDefaultValue; }
    if (const UBlackboardKeyType_Name* Name = Cast<UBlackboardKeyType_Name>(KeyType)) { OutText = Name->DefaultValue.ToString(); return true; }
    if (const UBlackboardKeyType_String* String = Cast<UBlackboardKeyType_String>(KeyType)) { OutText = String->DefaultValue; return true; }
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    if (const UBlackboardKeyType_Enum* Enum = Cast<UBlackboardKeyType_Enum>(KeyType))
    {
        OutText = Enum->EnumType ? Enum->EnumType->GetNameStringByValue(Enum->DefaultValue) : FString::FromInt(Enum->DefaultValue);
        return true;
    }
    if (const UBlackboardKeyType_Object* Object = Cast<UBlackboardKeyType_Object>(KeyType)) { OutText = GetPathNameSafe(Object->DefaultValue.Get()); return true; }
    if (const UBlackboardKeyType_Class* Class = Cast<UBlackboardKeyType_Class>(KeyType)) { OutText = GetPathNameSafe(Class->DefaultValue.Get()); return true; }
#endif
    return false;
}
}
