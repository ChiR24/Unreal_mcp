#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSimplifyMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    double TargetPercentage = GetNumberFieldGeom(Payload, TEXT("targetPercentage"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
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

    // UE 5.7: Use FGeometryScriptSimplifyMeshOptions (renamed from FGeometryScriptMeshSimplifyOptions)
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    // Note: bPreserveSharpEdges was removed in UE 5.7
    SimplifyOptions.bAllowSeamCollapse = true;

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    int32 TargetTriCount = FMath::Max(1, FMath::RoundToInt(TriCountBefore * (TargetPercentage / 100.0)));

    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh,
        TargetTriCount,
        SimplifyOptions,
        nullptr
    );

    int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("simplifiedTriangles"), TriCountAfter);
    Result->SetNumberField(TEXT("reductionPercent"), (1.0 - ((double)TriCountAfter / (double)TriCountBefore)) * 100.0);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh simplified"), Result);
    return true;
}

bool HandleSubdivide(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 Iterations = GetIntFieldGeom(Payload, TEXT("iterations"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Safety: Clamp iterations to prevent polygon explosion
    int32 OriginalIterations = Iterations;
    Iterations = FMath::Clamp(Iterations, 1, MAX_SUBDIVIDE_ITERATIONS);
    if (Iterations != OriginalIterations)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Subdivide iterations clamped from %d to %d (MAX_SUBDIVIDE_ITERATIONS)"),
               OriginalIterations, Iterations);
    }

    // Check memory pressure before heavy operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Subdivide blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
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

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    // Safety: Estimate triangles after subdivision and check against limit
    // Each subdivision iteration roughly quadruples triangle count
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore);
    for (int32 i = 0; i < Iterations; ++i)
    {
        EstimatedTriangles *= 4;  // Each subdivision ~4x triangles
    }

    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Subdivide would exceed triangle limit. Current: %d, Estimated after: %lld, Max allowed: %d"),
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    for (int32 i = 0; i < Iterations; ++i)
    {
        // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
        FGeometryScriptPNTessellateOptions TessOptions;
        UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);
    }

    int32 TriCountAfter = Mesh->GetTriangleCount();

    // Warning if approaching limit
    if (TriCountAfter > WARNING_TRIANGLE_THRESHOLD)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Subdivide result has %d triangles (warning threshold: %d)"),
               TriCountAfter, WARNING_TRIANGLE_THRESHOLD);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("subdividedTriangles"), TriCountAfter);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh subdivided"), Result);
    return true;
}

bool HandleRemeshUniform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 TargetTriangleCount = GetIntFieldGeom(Payload, TEXT("targetTriangleCount"), 5000);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
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

    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;

    FGeometryScriptUniformRemeshOptions UniformOptions;
    UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
    UniformOptions.TargetTriangleCount = TargetTriangleCount;

    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(
        Mesh, RemeshOptions, UniformOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("targetTriangleCount"), TargetTriangleCount);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Uniform remesh applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Collision Generation
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
