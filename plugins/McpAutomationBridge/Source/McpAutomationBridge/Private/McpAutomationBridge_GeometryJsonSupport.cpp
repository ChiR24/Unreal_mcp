#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
FVector ReadVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FVector Default)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [x, y, z]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FVector(
            (*ArrayPtr)[0]->AsNumber(),
            (*ArrayPtr)[1]->AsNumber(),
            (*ArrayPtr)[2]->AsNumber()
        );
    }

    // Try object format {x, y, z}
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        return FVector(
            GetNumberFieldGeom((*ObjPtr), TEXT("x")),
            GetNumberFieldGeom((*ObjPtr), TEXT("y")),
            GetNumberFieldGeom((*ObjPtr), TEXT("z"))
        );
    }

    return Default;
}

// Helper to read FRotator from JSON (supports both {pitch,yaw,roll} and {x,y,z} formats)

FRotator ReadRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FRotator Default)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [pitch, yaw, roll]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FRotator(
            (*ArrayPtr)[0]->AsNumber(),  // Pitch
            (*ArrayPtr)[1]->AsNumber(),  // Yaw
            (*ArrayPtr)[2]->AsNumber()   // Roll
        );
    }

    // Try object format {pitch, yaw, roll} or {x, y, z}
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        // Check for {pitch, yaw, roll} format first
        if ((*ObjPtr)->HasField(TEXT("pitch")) || (*ObjPtr)->HasField(TEXT("yaw")) || (*ObjPtr)->HasField(TEXT("roll")))
        {
            return FRotator(
                GetNumberFieldGeom((*ObjPtr), TEXT("pitch"), 0.0),
                GetNumberFieldGeom((*ObjPtr), TEXT("yaw"), 0.0),
                GetNumberFieldGeom((*ObjPtr), TEXT("roll"), 0.0)
            );
        }
        // Fallback to {x, y, z} format (x=Pitch, y=Yaw, z=Roll)
        return FRotator(
            GetNumberFieldGeom((*ObjPtr), TEXT("x")),
            GetNumberFieldGeom((*ObjPtr), TEXT("y")),
            GetNumberFieldGeom((*ObjPtr), TEXT("z"))
        );
    }

    return Default;
}

// Helper to read FTransform from JSON

FTransform ReadTransformFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
    FVector Location = ReadVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ReadRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Scale = ReadVectorFromPayload(Payload, TEXT("scale"), FVector::OneVector);

    return FTransform(
        Rotation,
        Location,
        Scale
    );
}

// Helper to create or get a dynamic mesh for operations

FVector AxisVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FVector& Default)
{
    const FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    if (Axis == TEXT("X")) return FVector::ForwardVector;
    if (Axis == TEXT("Y")) return FVector::RightVector;
    return Default;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
