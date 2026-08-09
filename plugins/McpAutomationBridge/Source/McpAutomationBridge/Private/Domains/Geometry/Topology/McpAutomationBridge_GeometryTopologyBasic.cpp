#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleTriangulate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

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

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Triangulation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    int32 TriCountBefore = Mesh->GetTriangleCount();
    if (TriCountBefore > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Mesh has too many triangles (%d). Max allowed: %d"),
                           TriCountBefore, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // Triangulate the mesh (convert quads/n-gons to triangles)
    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh, TriCountBefore, FGeometryScriptSimplifyMeshOptions(), nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh triangulated"), Result);
    return true;
}

bool HandlePoke(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double PokeOffset = GetJsonNumberField(Payload, TEXT("offset"), 0.0);

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

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Poke operation blocked to prevent OOM."),
                           GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Poke with PNTessellation roughly triples triangle count (each face gets subdivided)
    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * 4;  // 4x safety margin for subdivision

    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Poke would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"),
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions PokeOptions;
    PokeOptions.Distance = PokeOffset;
    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, PokeOptions, FGeometryScriptMeshSelection(), nullptr);

    // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
    FGeometryScriptPNTessellateOptions TessOptions;
    UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);

    int32 TriCountAfter = Mesh->GetTriangleCount();

    if (TriCountAfter > WARNING_TRIANGLE_THRESHOLD)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Poke result has %d triangles (warning threshold: %d)"),
               TriCountAfter, WARNING_TRIANGLE_THRESHOLD);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("offset"), PokeOffset);
    Result->SetNumberField(TEXT("triangleCount"), TriCountAfter);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Poke applied"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
