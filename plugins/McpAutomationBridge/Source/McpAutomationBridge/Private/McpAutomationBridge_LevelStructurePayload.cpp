#include "McpAutomationBridge_LevelStructurePayload.h"

#include "McpAutomationBridgeHelpersJsonFields.h"

namespace LevelStructureHelpers
{
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
    {
        return Payload->GetObjectField(FieldName);
    }
    return nullptr;
}

FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FVector Default)
{
    if (!JsonObj.IsValid())
    {
        return Default;
    }
    return FVector(
        GetJsonNumberField(JsonObj, TEXT("x"), Default.X),
        GetJsonNumberField(JsonObj, TEXT("y"), Default.Y),
        GetJsonNumberField(JsonObj, TEXT("z"), Default.Z));
}

FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FRotator Default)
{
    if (!JsonObj.IsValid())
    {
        return Default;
    }
    return FRotator(
        GetJsonNumberField(JsonObj, TEXT("pitch"), Default.Pitch),
        GetJsonNumberField(JsonObj, TEXT("yaw"), Default.Yaw),
        GetJsonNumberField(JsonObj, TEXT("roll"), Default.Roll));
}
}
