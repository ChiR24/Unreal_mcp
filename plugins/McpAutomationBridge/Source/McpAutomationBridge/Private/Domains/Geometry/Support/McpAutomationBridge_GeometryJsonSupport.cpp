#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
FTransform ReadTransformFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
    return FTransform(ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator),
                      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector),
                      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector));
}

FVector AxisVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FVector& Default)
{
    const FString Axis = GetJsonStringField(Payload, TEXT("axis")).ToUpper();
    if (Axis == TEXT("X")) return FVector::ForwardVector;
    if (Axis == TEXT("Y")) return FVector::RightVector;
    if (Axis == TEXT("Z")) return FVector::UpVector;
    return Default;
}

int32 AxisIndexFromPayload(const TSharedPtr<FJsonObject>& Payload, int32 Default)
{
    const FVector Axis = AxisVectorFromPayload(Payload, FVector::ZeroVector);
    return Axis.IsZero() ? Default : (Axis.X != 0.0 ? 0 : (Axis.Y != 0.0 ? 1 : 2));
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
