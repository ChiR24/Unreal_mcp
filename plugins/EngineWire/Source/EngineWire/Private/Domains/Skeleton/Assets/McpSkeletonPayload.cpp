#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Safety/McpSafeOperations.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpSkeletonHandlers
{

int32 ApplyTransformFieldsFromJson(const TSharedPtr<FJsonObject>& Payload, FTransform& InOutTransform)
{
    if (!Payload.IsValid())
    {
        return 0;
    }

    int32 Applied = 0;
    if (Payload->HasField(TEXT("location")))
    {
        InOutTransform.SetLocation(ExtractVectorField(Payload, TEXT("location"), InOutTransform.GetLocation()));
        ++Applied;
    }
    if (Payload->HasField(TEXT("rotation")))
    {
        InOutTransform.SetRotation(ExtractRotatorField(Payload, TEXT("rotation"), InOutTransform.Rotator()).Quaternion());
        ++Applied;
    }
    if (Payload->HasField(TEXT("scale")))
    {
        double UniformScale = 1.0;
        if (Payload->TryGetNumberField(TEXT("scale"), UniformScale))
        {
            InOutTransform.SetScale3D(FVector(UniformScale));
        }
        else
        {
            InOutTransform.SetScale3D(ExtractVectorField(Payload, TEXT("scale"), InOutTransform.GetScale3D()));
        }
        ++Applied;
    }
    return Applied;
}

void WriteTransformToJson(const FTransform& Transform, const TSharedPtr<FJsonObject>& Target)
{
    if (!Target.IsValid())
    {
        return;
    }
    const TSharedPtr<FJsonObject> Fields = McpHandlerUtils::TransformToJson(Transform);
    for (const auto& Field : Fields->Values)
    {
        Target->SetField(Field.Key, Field.Value);
    }
}

bool SaveIfRequested(UObject* Asset, const TSharedPtr<FJsonObject>& Payload)
{
    return !GetJsonBoolField(Payload, TEXT("save"), true) || McpSafeAssetSave(Asset);
}

void SaveAndReport(UObject* Asset, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
    Result->SetBoolField(TEXT("saved"), GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(Asset));
    TSharedPtr<FJsonObject> Target = Result;
    McpHandlerUtils::AddVerification(Target, Asset);
}
}
