#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Safety/McpSafeOperations.h"

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

    const FVector Location = Transform.GetLocation();
    Target->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Location));

    const FRotator Rotation = Transform.Rotator();
    Target->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(Rotation));

    const FVector Scale = Transform.GetScale3D();
    Target->SetObjectField(TEXT("scale"), McpHandlerUtils::VectorToJson(Scale));
}

bool SaveIfRequested(UObject* Asset, const TSharedPtr<FJsonObject>& Payload)
{
    bool bSave = true;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(TEXT("save"), bSave);
    }
    return !bSave || McpSafeAssetSave(Asset);
}
}
