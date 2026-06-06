#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleBooleanOperation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                   const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   EGeometryScriptBooleanOperation BoolOp, const FString& OpName)
{
    FString TargetActorName = GetStringFieldGeom(Payload, TEXT("targetActor"));
    FString ToolActorName = GetStringFieldGeom(Payload, TEXT("toolActor"));
    bool bKeepTool = GetBoolFieldGeom(Payload, TEXT("keepTool"), true);  // Default to true to prevent cascade test failures

    if (TargetActorName.IsEmpty() || ToolActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetActor and toolActor required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    // Find target and tool actors
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* ToolActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == TargetActorName)
            TargetActor = *It;
        if (It->GetActorLabel() == ToolActorName)
            ToolActor = *It;
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    if (!ToolActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Tool actor not found: %s"), *ToolActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* TargetDMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* ToolDMC = ToolActor->GetDynamicMeshComponent();

    if (!TargetDMC || !ToolDMC)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMeshComponent not found on actors"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* TargetMesh = TargetDMC->GetDynamicMesh();
    UDynamicMesh* ToolMesh = ToolDMC->GetDynamicMesh();

    if (!TargetMesh || !ToolMesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // Get triangle counts before operation for validation
    int32 TargetTriCount = TargetMesh->GetTriangleCount();
    int32 ToolTriCount = ToolMesh->GetTriangleCount();
    int64 EstimatedMaxTriangles = static_cast<int64>(TargetTriCount) + static_cast<int64>(ToolTriCount);

    // Safety: Check memory pressure before heavy operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Boolean %s blocked to prevent OOM."),
                           GetMemoryUsagePercent(), *OpName),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate maximum possible triangles and check against limit
    // Boolean operations can at most combine both meshes, but may create additional geometry
    int64 EstimatedWithSafetyMargin = EstimatedMaxTriangles * 3;  // 3x safety margin for intersection edges
    if (EstimatedWithSafetyMargin > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Boolean %s would exceed polygon limit. Target: %d, Tool: %d, Estimated max: %lld, Limit: %d"),
                           *OpName, TargetTriCount, ToolTriCount, EstimatedWithSafetyMargin, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;
    BoolOptions.bSimplifyOutput = false;

    // UE 5.7: ApplyMeshBoolean returns UDynamicMesh* directly, no Outcome parameter
    UDynamicMesh* ResultMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh,
        TargetActor->GetActorTransform(),
        ToolMesh,
        ToolActor->GetActorTransform(),
        BoolOp,
        BoolOptions,
        nullptr
    );

    bool bBooleanSucceeded = (ResultMesh != nullptr);

    // Safety: Check result polygon count
    int32 ResultTriCount = 0;
    if (ResultMesh)
    {
        ResultTriCount = ResultMesh->GetTriangleCount();

        // Check if result exceeds limit
        if (ResultTriCount > MAX_TRIANGLES_PER_DYNAMIC_MESH)
        {
            // Log warning but don't fail - the operation already completed
            UE_LOG(LogMcpGeometryHandlers, Warning,
                   TEXT("Boolean %s result has %d triangles (exceeds limit of %d)"),
                   *OpName, ResultTriCount, MAX_TRIANGLES_PER_DYNAMIC_MESH);
        }

        // Warning if approaching limit
        if (ResultTriCount > WARNING_TRIANGLE_THRESHOLD)
        {
            UE_LOG(LogMcpGeometryHandlers, Warning,
                   TEXT("Boolean %s result has %d triangles (warning threshold: %d)"),
                   *OpName, ResultTriCount, WARNING_TRIANGLE_THRESHOLD);
        }
    }
    else
    {
        // Boolean operation returned null - this typically means the operation failed
        // (e.g., empty result from intersection, non-overlapping meshes)
        UE_LOG(LogMcpGeometryHandlers, Warning,
               TEXT("Boolean %s returned null result - operation may have produced empty geometry"), *OpName);
    }

    // Optionally delete tool actor
    if (!bKeepTool)
    {
        ToolActor->Destroy();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetActor"), TargetActorName);
    Result->SetStringField(TEXT("operation"), OpName);
    Result->SetBoolField(TEXT("success"), bBooleanSucceeded);
    Result->SetNumberField(TEXT("targetTriangles"), TargetTriCount);
    Result->SetNumberField(TEXT("toolTriangles"), ToolTriCount);
    if (bBooleanSucceeded)
    {
        Result->SetNumberField(TEXT("resultTriangles"), ResultTriCount);
    }

    Self->SendAutomationResponse(Socket, RequestId, bBooleanSucceeded,
        bBooleanSucceeded ? FString::Printf(TEXT("Boolean %s completed"), *OpName) : FString::Printf(TEXT("Boolean %s failed - operation produced empty geometry"), *OpName),
        Result);
    return true;
}

bool HandleBooleanUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Union, TEXT("Union"));
}

bool HandleBooleanSubtract(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Subtract, TEXT("Subtract"));
}

bool HandleBooleanIntersection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Intersection, TEXT("Intersection"));
}

// -------------------------------------------------------------------------
// Mesh Utils
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
