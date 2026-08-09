#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
FVector ReadVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FVector Default)
{
    if (!Payload.IsValid())
        return Default;

    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FVector(
            (*ArrayPtr)[0]->AsNumber(),
            (*ArrayPtr)[1]->AsNumber(),
            (*ArrayPtr)[2]->AsNumber()
        );
    }

    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        return FVector(
            GetJsonNumberField((*ObjPtr), TEXT("x")),
            GetJsonNumberField((*ObjPtr), TEXT("y")),
            GetJsonNumberField((*ObjPtr), TEXT("z"))
        );
    }

    return Default;
}

// Helper to read FRotator from JSON (supports both {pitch,yaw,roll} and {x,y,z} formats)

FRotator ReadRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FRotator Default)
{
    if (!Payload.IsValid())
        return Default;

    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FRotator(
            (*ArrayPtr)[0]->AsNumber(),  // Pitch
            (*ArrayPtr)[1]->AsNumber(),  // Yaw
            (*ArrayPtr)[2]->AsNumber()   // Roll
        );
    }

    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        if ((*ObjPtr)->HasField(TEXT("pitch")) || (*ObjPtr)->HasField(TEXT("yaw")) || (*ObjPtr)->HasField(TEXT("roll")))
        {
            return FRotator(
                GetJsonNumberField((*ObjPtr), TEXT("pitch"), 0.0),
                GetJsonNumberField((*ObjPtr), TEXT("yaw"), 0.0),
                GetJsonNumberField((*ObjPtr), TEXT("roll"), 0.0)
            );
        }
        // Fallback to {x, y, z} format (x=Pitch, y=Yaw, z=Roll)
        return FRotator(
            GetJsonNumberField((*ObjPtr), TEXT("x")),
            GetJsonNumberField((*ObjPtr), TEXT("y")),
            GetJsonNumberField((*ObjPtr), TEXT("z"))
        );
    }

    return Default;
}

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

FVector AxisVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FVector& Default)
{
    const FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    if (Axis == TEXT("X")) return FVector::ForwardVector;
    if (Axis == TEXT("Y")) return FVector::RightVector;
    return Default;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
