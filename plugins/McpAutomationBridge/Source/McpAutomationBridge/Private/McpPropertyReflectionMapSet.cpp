#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
namespace Private
{
namespace
{
FString MapKeyToString(FProperty* KeyProp, const uint8* KeyPtr, int32 Index)
{
    if (CastField<FStrProperty>(KeyProp)) return *reinterpret_cast<const FString*>(KeyPtr);
    if (CastField<FNameProperty>(KeyProp)) return reinterpret_cast<const FName*>(KeyPtr)->ToString();
    if (CastField<FIntProperty>(KeyProp)) return FString::FromInt(*reinterpret_cast<const int32*>(KeyPtr));
    return FString::Printf(TEXT("key_%d"), Index);
}

void SetMapValueField(TSharedPtr<FJsonObject> MapObj, const FString& Key, FProperty* ValueProp, const uint8* ValuePtr)
{
    if (CastField<FStrProperty>(ValueProp))
    {
        MapObj->SetStringField(Key, *reinterpret_cast<const FString*>(ValuePtr));
    }
    else if (CastField<FIntProperty>(ValueProp))
    {
        MapObj->SetNumberField(Key, static_cast<double>(*reinterpret_cast<const int32*>(ValuePtr)));
    }
    else if (CastField<FFloatProperty>(ValueProp))
    {
        MapObj->SetNumberField(Key, static_cast<double>(*reinterpret_cast<const float*>(ValuePtr)));
    }
    else if (CastField<FBoolProperty>(ValueProp))
    {
        MapObj->SetBoolField(Key, (*reinterpret_cast<const uint8*>(ValuePtr)) != 0);
    }
    else
    {
        FString ValueStr;
        MCP_PROPERTY_EXPORT_TEXT(ValueProp, ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
        MapObj->SetStringField(Key, ValueStr);
    }
}

TSharedPtr<FJsonValue> SetElementToJsonValue(FProperty* ElemProp, const uint8* ElemPtr)
{
    if (CastField<FStrProperty>(ElemProp)) return MakeShared<FJsonValueString>(*reinterpret_cast<const FString*>(ElemPtr));
    if (CastField<FNameProperty>(ElemProp)) return MakeShared<FJsonValueString>(reinterpret_cast<const FName*>(ElemPtr)->ToString());
    if (CastField<FIntProperty>(ElemProp)) return MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<const int32*>(ElemPtr)));
    if (CastField<FFloatProperty>(ElemProp)) return MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<const float*>(ElemPtr)));

    FString ElemStr;
    MCP_PROPERTY_EXPORT_TEXT(ElemProp, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
    return MakeShared<FJsonValueString>(ElemStr);
}
}

TSharedPtr<FJsonValue> ExportMapToJsonValue(void* TargetContainer, FMapProperty* MapProp)
{
    TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
    FScriptMapHelper Helper(MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        if (!Helper.IsValidIndex(i)) continue;
        SetMapValueField(
            MapObj,
            MapKeyToString(MapProp->KeyProp, Helper.GetKeyPtr(i), i),
            MapProp->ValueProp,
            Helper.GetValuePtr(i));
    }

    return MakeShared<FJsonValueObject>(MapObj);
}

TSharedPtr<FJsonValue> ExportSetToJsonValue(void* TargetContainer, FSetProperty* SetProp)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    FScriptSetHelper Helper(SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        if (Helper.IsValidIndex(i))
        {
            Out.Add(SetElementToJsonValue(SetProp->ElementProp, Helper.GetElementPtr(i)));
        }
    }

    return MakeShared<FJsonValueArray>(Out);
}
}
}
