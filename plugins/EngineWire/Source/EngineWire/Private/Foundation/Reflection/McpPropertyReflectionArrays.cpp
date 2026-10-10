#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
namespace Private
{
TSharedPtr<FJsonValue> ExportElementToJsonValue(FProperty* Inner, void* ElemPtr)
{
    // A container's inner property sits at offset 0, so the element is its own container.
    if (TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(ElemPtr, Inner))
    {
        return Value;
    }
    FString ElemStr;
    MCP_PROPERTY_EXPORT_TEXT(Inner, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
    return MakeShared<FJsonValueString>(ElemStr);
}
}

TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Container || !ArrayProp) return Out;

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        Out.Add(Private::ExportElementToJsonValue(ArrayProp->Inner, Helper.GetRawPtr(i)));
    }
    return Out;
}
}
