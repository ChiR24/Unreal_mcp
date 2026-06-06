#include "McpAutomationBridge_SkeletonHandlersPayload.h"

namespace McpSkeletonHandlers
{
int32 GetIntFieldSkel(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, int32 DefaultValue)
{
    return JsonObject.IsValid() && JsonObject->HasField(FieldName)
        ? static_cast<int32>(JsonObject->GetNumberField(FieldName))
        : DefaultValue;
}

FVector ParseVectorFromJson(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FString& FieldName,
    const FVector& Default)
{
    if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* VectorObject = nullptr;
    if (!JsonObject->TryGetObjectField(FieldName, VectorObject) || !VectorObject || !VectorObject->IsValid())
    {
        return Default;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    (*VectorObject)->TryGetNumberField(TEXT("x"), X);
    (*VectorObject)->TryGetNumberField(TEXT("y"), Y);
    (*VectorObject)->TryGetNumberField(TEXT("z"), Z);
    return FVector(X, Y, Z);
}

FRotator ParseRotatorFromJson(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FString& FieldName,
    const FRotator& Default)
{
    if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* RotatorObject = nullptr;
    if (!JsonObject->TryGetObjectField(FieldName, RotatorObject) || !RotatorObject || !RotatorObject->IsValid())
    {
        return Default;
    }

    double Pitch = 0.0;
    double Yaw = 0.0;
    double Roll = 0.0;
    (*RotatorObject)->TryGetNumberField(TEXT("pitch"), Pitch);
    (*RotatorObject)->TryGetNumberField(TEXT("yaw"), Yaw);
    (*RotatorObject)->TryGetNumberField(TEXT("roll"), Roll);
    return FRotator(Pitch, Yaw, Roll);
}
}
