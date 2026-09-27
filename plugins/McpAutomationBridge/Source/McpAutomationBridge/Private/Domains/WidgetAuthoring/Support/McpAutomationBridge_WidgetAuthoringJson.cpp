#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace WidgetAuthoringHelpers
{
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
    {
        return Payload->GetObjectField(FieldName);
    }
    return nullptr;
}

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
    if (!Payload.IsValid())
    {
        return FString();
    }
    // bind_widget publishes `targetWidget` as well and it reads as the obvious
    // way to name the button being bound, but only slotName/widgetName were
    // read - a contract-following call was refused for a parameter it had
    // supplied. `componentName` is the spelling the sibling add_* actions use.
    for (const TCHAR* Key : {TEXT("slotName"), TEXT("widgetName"),
                             TEXT("targetWidget"), TEXT("componentName")})
    {
        const FString Candidate = GetJsonStringField(Payload, Key);
        if (!Candidate.IsEmpty())
        {
            return Candidate;
        }
    }
    return FString();
}

ESlateVisibility GetVisibility(const FString& VisibilityStr)
{
    if (VisibilityStr.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
    {
        return ESlateVisibility::Collapsed;
    }
    if (VisibilityStr.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
    {
        return ESlateVisibility::Hidden;
    }
    if (VisibilityStr.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase))
    {
        return ESlateVisibility::HitTestInvisible;
    }
    if (VisibilityStr.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase))
    {
        return ESlateVisibility::SelfHitTestInvisible;
    }
    return ESlateVisibility::Visible;
}
}
