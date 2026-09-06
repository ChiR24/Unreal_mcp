#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

namespace McpSkeletonHandlers
{

FVector ParseVectorFromJson(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FString& FieldName,
    const FVector& Default)
{
    // ExtractVectorField reads {x,y,z} objects and [x,y,z] arrays. The older
    // object-only parser ignored the array form the records document, so a
    // location:[8.73,0,0] edit reported success and changed nothing (#93).
    return ExtractVectorField(JsonObject, *FieldName, Default);
}

FRotator ParseRotatorFromJson(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FString& FieldName,
    const FRotator& Default)
{
    return ExtractRotatorField(JsonObject, *FieldName, Default);
}

int32 ApplyTransformFieldsFromJson(const TSharedPtr<FJsonObject>& Payload, FTransform& InOutTransform)
{
    if (!Payload.IsValid())
    {
        return 0;
    }

    int32 Applied = 0;
    if (Payload->HasField(TEXT("location")))
    {
        InOutTransform.SetLocation(ParseVectorFromJson(Payload, TEXT("location"), InOutTransform.GetLocation()));
        ++Applied;
    }
    if (Payload->HasField(TEXT("rotation")))
    {
        InOutTransform.SetRotation(ParseRotatorFromJson(Payload, TEXT("rotation"), InOutTransform.Rotator()).Quaternion());
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
            InOutTransform.SetScale3D(ParseVectorFromJson(Payload, TEXT("scale"), InOutTransform.GetScale3D()));
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
    TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
    LocationObj->SetNumberField(TEXT("x"), Location.X);
    LocationObj->SetNumberField(TEXT("y"), Location.Y);
    LocationObj->SetNumberField(TEXT("z"), Location.Z);
    Target->SetObjectField(TEXT("location"), LocationObj);

    const FRotator Rotation = Transform.Rotator();
    TSharedPtr<FJsonObject> RotationObj = MakeShared<FJsonObject>();
    RotationObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotationObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotationObj->SetNumberField(TEXT("roll"), Rotation.Roll);
    Target->SetObjectField(TEXT("rotation"), RotationObj);

    const FVector Scale = Transform.GetScale3D();
    TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
    ScaleObj->SetNumberField(TEXT("x"), Scale.X);
    ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
    ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
    Target->SetObjectField(TEXT("scale"), ScaleObj);
}
}
