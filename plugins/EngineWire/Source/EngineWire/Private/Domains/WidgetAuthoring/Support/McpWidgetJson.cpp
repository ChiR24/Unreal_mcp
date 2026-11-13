#include "Domains/WidgetAuthoring/McpWidgetPayload.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

namespace WidgetAuthoringHelpers
{
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
    {
        return &Payload->GetArrayField(FieldName);
    }
    return nullptr;
}

FString GetSlotName(const TSharedPtr<FJsonObject>& Payload)
{
    // slotName: the one spelling the records routed here declare.
    return Payload.IsValid() ? GetJsonStringField(Payload, TEXT("slotName")) : FString();
}

bool TryParseVisibility(const FString& Name, ESlateVisibility& Out)
{
    static const TPair<const TCHAR*, ESlateVisibility> Names[] = {
        { TEXT("Visible"), ESlateVisibility::Visible }, { TEXT("Collapsed"), ESlateVisibility::Collapsed },
        { TEXT("Hidden"), ESlateVisibility::Hidden }, { TEXT("HitTestInvisible"), ESlateVisibility::HitTestInvisible },
        { TEXT("SelfHitTestInvisible"), ESlateVisibility::SelfHitTestInvisible } };
    for (const TPair<const TCHAR*, ESlateVisibility>& Entry : Names)
    {
        if (Name.Equals(Entry.Key, ESearchCase::IgnoreCase))
        {
            Out = Entry.Value;
            return true;
        }
    }
    return false;
}
}
