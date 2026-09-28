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

ESlateVisibility GetVisibility(const FString& VisibilityStr)
{
    ESlateVisibility Visibility = ESlateVisibility::Visible;
    TryParseVisibility(VisibilityStr, Visibility);
    return Visibility;
}
}
