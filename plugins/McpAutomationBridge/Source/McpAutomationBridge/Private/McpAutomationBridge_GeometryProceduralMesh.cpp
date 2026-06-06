#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleCreateProceduralMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetStringFieldGeom(Payload, TEXT("name"));
    if (Name.IsEmpty()) Name = TEXT("ProceduralMesh");

    FTransform Transform = ReadTransformFromPayload(Payload);
    bool bEnableCollision = GetBoolFieldGeom(Payload, TEXT("enableCollision"), false);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    // Initialize with an empty dynamic mesh
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent();
        if (DMComp)
        {
            DMComp->SetGenerateOverlapEvents(bEnableCollision);
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetBoolField(TEXT("enableCollision"), bEnableCollision);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Procedural mesh actor created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// append_triangle - Add single triangle to mesh
// -------------------------------------------------------------------------

bool HandleAppendTriangle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Read vertices
    FVector V0 = ReadVectorFromPayload(Payload, TEXT("v0"), FVector(0, 0, 0));
    FVector V1 = ReadVectorFromPayload(Payload, TEXT("v1"), FVector(100, 0, 0));
    FVector V2 = ReadVectorFromPayload(Payload, TEXT("v2"), FVector(50, 100, 0));
    int32 GroupID = GetIntFieldGeom(Payload, TEXT("groupID"), 0);

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

    // Use the internal mesh directly to append triangle
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    // Append vertices
    int32 Idx0 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V0));
    int32 Idx1 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V1));
    int32 Idx2 = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(V2));

    // Append triangle
    int32 TriIdx = EditMesh.AppendTriangle(Idx0, Idx1, Idx2, GroupID);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleIndex"), TriIdx);
    Result->SetNumberField(TEXT("vertexIndex0"), Idx0);
    Result->SetNumberField(TEXT("vertexIndex1"), Idx1);
    Result->SetNumberField(TEXT("vertexIndex2"), Idx2);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Triangle appended"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_vertex_color - Set vertex colors on mesh
// -------------------------------------------------------------------------

bool HandleDeleteTriangle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 TriangleIndex = GetIntFieldGeom(Payload, TEXT("triangleIndex"), -1);

    if (ActorName.IsEmpty() || TriangleIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and triangleIndex required"), TEXT("INVALID_ARGUMENT"));
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    if (!EditMesh.IsTriangle(TriangleIndex))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid triangle index: %d"), TriangleIndex), TEXT("INVALID_TRIANGLE"));
        return true;
    }

    UE::Geometry::EMeshResult RemoveResult = EditMesh.RemoveTriangle(TriangleIndex);
    bool bSuccess = (RemoveResult == UE::Geometry::EMeshResult::Ok);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleIndex"), TriangleIndex);
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Triangle deleted"), Result);
    return true;
}

// -------------------------------------------------------------------------
// get_vertex_position - Get position of a vertex
// -------------------------------------------------------------------------

bool HandleSetVertexColor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    double R = GetNumberFieldGeom(Payload, TEXT("r"), 1.0);
    double G = GetNumberFieldGeom(Payload, TEXT("g"), 1.0);
    double B = GetNumberFieldGeom(Payload, TEXT("b"), 1.0);
    double A = GetNumberFieldGeom(Payload, TEXT("a"), 1.0);
    bool bSetAll = GetBoolFieldGeom(Payload, TEXT("setAll"), false);

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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    // Enable vertex colors if not already enabled
    if (!EditMesh.HasVertexColors())
    {
        EditMesh.EnableVertexColors(FVector3f(1.0f, 1.0f, 1.0f));
    }

    FVector4f Color(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
    int32 VerticesModified = 0;

    if (bSetAll)
    {
        // Set all vertex colors
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            EditMesh.SetVertexColor(VID, Color);
            VerticesModified++;
        }
    }
    else if (VertexIndex >= 0 && EditMesh.IsVertex(VertexIndex))
    {
        EditMesh.SetVertexColor(VertexIndex, Color);
        VerticesModified = 1;
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Result->SetNumberField(TEXT("r"), R);
    Result->SetNumberField(TEXT("g"), G);
    Result->SetNumberField(TEXT("b"), B);
    Result->SetNumberField(TEXT("a"), A);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex color set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_uvs - Set UV coordinates on mesh
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
