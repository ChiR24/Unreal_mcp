#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

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
}

TSharedPtr<FJsonValue> ExportMapToJsonValue(void* TargetContainer, FMapProperty* MapProp)
{
    TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
    FScriptMapHelper Helper(MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
    {
        if (!Helper.IsValidIndex(i)) continue;
        MapObj->SetField(
            MapKeyToString(MapProp->KeyProp, Helper.GetKeyPtr(i), i),
            ExportElementToJsonValue(MapProp->ValueProp, Helper.GetValuePtr(i)));
    }

    return MakeShared<FJsonValueObject>(MapObj);
}

TSharedPtr<FJsonValue> ExportSetToJsonValue(void* TargetContainer, FSetProperty* SetProp)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    FScriptSetHelper Helper(SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));

    for (int32 i = 0; i < Helper.GetMaxIndex(); ++i)
    {
        if (Helper.IsValidIndex(i))
        {
            Out.Add(ExportElementToJsonValue(SetProp->ElementProp, Helper.GetElementPtr(i)));
        }
    }

    return MakeShared<FJsonValueArray>(Out);
}
}
}
