#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
int32 GetArrayPropertyCount(void* Container, FArrayProperty* ArrayProp)
{
    if (!Container || !ArrayProp) return 0;
    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    return Helper.Num();
}

TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Container || !ArrayProp) return Out;

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        void* ElemPtr = Helper.GetRawPtr(i);
        FProperty* Inner = ArrayProp->Inner;

        if (CastField<FStrProperty>(Inner)) Out.Add(MakeShared<FJsonValueString>(*reinterpret_cast<FString*>(ElemPtr)));
        else if (CastField<FNameProperty>(Inner)) Out.Add(MakeShared<FJsonValueString>(reinterpret_cast<FName*>(ElemPtr)->ToString()));
        else if (FTextProperty* TextInner = CastField<FTextProperty>(Inner)) Out.Add(MakeShared<FJsonValueString>(Private::ExportTextToJsonString(TextInner->GetPropertyValue(ElemPtr))));
        else if (CastField<FBoolProperty>(Inner)) Out.Add(MakeShared<FJsonValueBoolean>((*reinterpret_cast<uint8*>(ElemPtr)) != 0));
        else if (CastField<FFloatProperty>(Inner)) Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<float*>(ElemPtr))));
        else if (CastField<FDoubleProperty>(Inner)) Out.Add(MakeShared<FJsonValueNumber>(*reinterpret_cast<double*>(ElemPtr)));
        else if (CastField<FIntProperty>(Inner)) Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<int32*>(ElemPtr))));
        else if (CastField<FInt64Property>(Inner)) Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<int64*>(ElemPtr))));
        else if (FByteProperty* ByteInner = CastField<FByteProperty>(Inner))
        {
            const uint8 ByteVal = *reinterpret_cast<uint8*>(ElemPtr);
            const FString EnumName = ByteInner->Enum ? ByteInner->Enum->GetNameStringByValue(ByteVal) : FString();
            if (!EnumName.IsEmpty())
            {
                Out.Add(MakeShared<FJsonValueString>(EnumName));
            }
            else
            {
                Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(ByteVal)));
            }
        }
        else
        {
            FString ElemStr;
            MCP_PROPERTY_EXPORT_TEXT(Inner, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
            Out.Add(MakeShared<FJsonValueString>(ElemStr));
        }
    }

    return Out;
}

bool ImportJsonToArray(void* Container, FArrayProperty* ArrayProp, const TArray<TSharedPtr<FJsonValue>>& JsonArray, FString& OutError)
{
    if (!Container || !ArrayProp)
    {
        OutError = TEXT("Invalid container or array property");
        return false;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    Helper.EmptyValues();
    Helper.Resize(JsonArray.Num());

    FProperty* Inner = ArrayProp->Inner;
    for (int32 i = 0; i < JsonArray.Num(); ++i)
    {
        uint8* ElemPtr = Helper.GetRawPtr(i);
        Inner->InitializeValue(ElemPtr);

        FString PropError;
        if (!McpPropertyReflection::ApplyJsonValueToProperty(ElemPtr, Inner, JsonArray[i], PropError))
        {
            OutError = FString::Printf(TEXT("Failed to set array element %d: %s"), i, *PropError);
            return false;
        }
    }

    return true;
}
}
