#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

// Reads `<ArrayField>` (array of strings) plus the optional single-value
// `<SingleField>` alias into one list. Shared by the property-dump paths of
// inspect_object and get_component_details.
TArray<FString> McpReadStringListField(const TSharedPtr<FJsonObject> &Payload,
                                       const TCHAR *ArrayField, const TCHAR *SingleField)
{
    TArray<FString> Values;
    if (!Payload.IsValid())
    {
        return Values;
    }
    const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
    if (ArrayField && Payload->TryGetArrayField(ArrayField, Array) && Array)
    {
        for (const TSharedPtr<FJsonValue> &Value : *Array)
        {
            FString Text;
            if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
            {
                Values.AddUnique(Text);
            }
        }
    }
    FString Single;
    if (SingleField && Payload->TryGetStringField(SingleField, Single) && !Single.IsEmpty())
    {
        Values.AddUnique(Single);
    }
    return Values;
}

// UPROPERTY values as exported text (ExportText_InContainer through the shared
// reflection helper), optionally filtered by name and capped at 200 entries so
// a large asset cannot flood the response. `missingProperties` lists requested
// names the class does not declare.
void McpAppendPropertyDump(UObject *Object, const TArray<FString> &PropertyNames,
                           TSharedPtr<FJsonObject> Resp)
{
    if (!Object || !Resp.IsValid())
    {
        return;
    }
    constexpr int32 MaxProperties = 200;
    TSet<FString> Wanted;
    for (const FString &Name : PropertyNames)
    {
        Wanted.Add(Name.ToLower());
    }
    TSharedPtr<FJsonObject> Properties = McpHandlerUtils::CreateResultObject();
    TSet<FString> Found;
    int32 Total = 0;
    int32 Emitted = 0;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty *Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            continue;
        }
        const FString Name = Property->GetName();
        if (Wanted.Num() > 0 && !Wanted.Contains(Name.ToLower()))
        {
            continue;
        }
        ++Total;
        if (Emitted >= MaxProperties)
        {
            continue;
        }
        Found.Add(Name.ToLower());
        Properties->SetStringField(Name, McpPropertyReflection::GetPropertyValueAsString(Object, Property));
        ++Emitted;
    }
    Resp->SetObjectField(TEXT("properties"), Properties);
    Resp->SetNumberField(TEXT("propertyCount"), Emitted);
    Resp->SetNumberField(TEXT("totalPropertyCount"), Total);
    Resp->SetBoolField(TEXT("propertiesTruncated"), Total > Emitted);
    if (Wanted.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Missing;
        for (const FString &Name : PropertyNames)
        {
            if (!Found.Contains(Name.ToLower()))
            {
                Missing.Add(MakeShared<FJsonValueString>(Name));
            }
        }
        Resp->SetArrayField(TEXT("missingProperties"), Missing);
    }
}

TSharedPtr<FJsonObject> McpMakeBoundsObject(const FBox &Box)
{
    TSharedPtr<FJsonObject> Bounds = McpHandlerUtils::CreateResultObject();
    const FVector Extent = Box.GetExtent();
    Bounds->SetObjectField(TEXT("origin"), McpMakeVectorObject(Box.GetCenter()));
    Bounds->SetObjectField(TEXT("extent"), McpMakeVectorObject(Extent));
    Bounds->SetObjectField(TEXT("min"), McpMakeVectorObject(Box.Min));
    Bounds->SetObjectField(TEXT("max"), McpMakeVectorObject(Box.Max));
    Bounds->SetObjectField(TEXT("size"), McpMakeVectorObject(Box.GetSize()));
    Bounds->SetNumberField(TEXT("radius"), Extent.Size());
    Bounds->SetBoolField(TEXT("isValid"), Box.IsValid != 0);
    return Bounds;
}

} // namespace McpEnvironmentHandlers
#endif
