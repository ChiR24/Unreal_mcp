#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {

bool McpGetFirstNumberField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields, double &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    for (const TCHAR *Field : Fields)
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Field, Value))
        {
            OutValue = Value;
            return true;
        }
    }
    return false;
}
void McpAddStringArrayField(TSharedPtr<FJsonObject> Obj, const TCHAR *FieldName, const TArray<FString> &Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString &Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    Obj->SetArrayField(FieldName, JsonValues);
}

} // namespace McpEnvironmentHandlers
