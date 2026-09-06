#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Count = GetJsonIntField(Payload, TEXT("count"), 3);
    FVector Offset = ReadVectorFromPayload(Payload, TEXT("offset"), FVector(100, 0, 0));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * Count;

    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Array would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"),
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    FTransform RepeatTransform;
    RepeatTransform.SetLocation(Offset);

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
        Mesh, SourceMesh, RepeatTransform, Count - 1, false, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear array applied"), Result);
    return true;
}

bool HandleArrayRadial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Count = GetJsonIntField(Payload, TEXT("count"), 6);
    FVector Center = ReadVectorFromPayload(Payload, TEXT("center"), FVector::ZeroVector);
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double TotalAngle = GetJsonNumberField(Payload, TEXT("angle"), 360.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * Count;

    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Array would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"),
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    double AngleStep = TotalAngle / Count;
    FVector RotationAxis = FVector::UpVector;
    if (Axis == TEXT("X")) RotationAxis = FVector::ForwardVector;
    else if (Axis == TEXT("Y")) RotationAxis = FVector::RightVector;

    TArray<FTransform> Transforms;
    for (int32 i = 1; i < Count; ++i)  // Start from 1 (original is at 0)
    {
        double Angle = AngleStep * i;
        FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(Angle));
        FTransform Transform;
        Transform.SetRotation(Rotation);
        Transform.SetLocation(Center + Rotation.RotateVector(-Center));
        Transforms.Add(Transform);
    }

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshTransformed(
        Mesh, SourceMesh, Transforms, FTransform::Identity, true, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("angle"), TotalAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Radial array applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
