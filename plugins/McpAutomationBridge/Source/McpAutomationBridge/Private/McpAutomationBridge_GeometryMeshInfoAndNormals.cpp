#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGetMeshInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

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
    int32 VertexCount = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    int32 TriangleCount = Mesh->GetTriangleCount();
    bool bHasNormals = UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleNormals(Mesh);
    int32 NumUVSets = UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets(Mesh);
    bool bHasVertexColors = UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexColors(Mesh);
    bool bHasMaterialIDs = UGeometryScriptLibrary_MeshQueryFunctions::GetHasMaterialIDs(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("triangleCount"), TriangleCount);
    Result->SetBoolField(TEXT("hasNormals"), bHasNormals);
    Result->SetBoolField(TEXT("hasUVs"), NumUVSets > 0);
    Result->SetBoolField(TEXT("hasColors"), bHasVertexColors);
    Result->SetBoolField(TEXT("hasPolygroups"), bHasMaterialIDs);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh info retrieved"), Result);
    return true;
}

bool HandleRecalculateNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    bool bAreaWeighted = GetBoolFieldGeom(Payload, TEXT("areaWeighted"), true);
    double SplitAngle = GetNumberFieldGeom(Payload, TEXT("splitAngle"), 60.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

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

    FGeometryScriptCalculateNormalsOptions NormalOptions;
    NormalOptions.bAreaWeighted = bAreaWeighted;
    NormalOptions.bAngleWeighted = true;

    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters (with bDeferChangeNotifications)
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
        Mesh,
        NormalOptions,
        false,  // bDeferChangeNotifications
        nullptr
    );
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
        Mesh,
        NormalOptions,
        nullptr
    );
#endif

    // Force refresh
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("areaWeighted"), bAreaWeighted);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals recalculated"), Result);
    return true;
}

bool HandleFlipNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
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

    UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Mesh, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals flipped"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
