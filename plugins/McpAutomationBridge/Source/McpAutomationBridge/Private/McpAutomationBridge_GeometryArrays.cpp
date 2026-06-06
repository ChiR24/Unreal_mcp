#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 3);
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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before array operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate triangles after array and check against limit
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

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Create transform for repeat
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
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Count = GetIntFieldGeom(Payload, TEXT("count"), 6);
    FVector Center = ReadVectorFromPayload(Payload, TEXT("center"), FVector::ZeroVector);
    FString Axis = GetStringFieldGeom(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double TotalAngle = GetNumberFieldGeom(Payload, TEXT("angle"), 360.0);

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

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before array operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate triangles after array and check against limit
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

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Calculate rotation per step
    double AngleStep = TotalAngle / Count;
    FVector RotationAxis = FVector::UpVector;
    if (Axis == TEXT("X")) RotationAxis = FVector::ForwardVector;
    else if (Axis == TEXT("Y")) RotationAxis = FVector::RightVector;

    // Build transforms array
    TArray<FTransform> Transforms;
    for (int32 i = 1; i < Count; ++i)  // Start from 1 (original is at 0)
    {
        double Angle = AngleStep * i;
        FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(Angle));
        FTransform Transform;
        Transform.SetRotation(Rotation);
        // Rotate around center point
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

// -------------------------------------------------------------------------
// Additional Primitives (Arch, Pipe)
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
