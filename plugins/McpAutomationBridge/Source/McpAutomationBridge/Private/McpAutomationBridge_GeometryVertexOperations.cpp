#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAppendVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FVector Position = ReadVectorFromPayload(Payload, TEXT("position"), FVector::ZeroVector);

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

    int32 VertexIndex = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(Position));

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("vertexCount"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex appended"), Result);
    return true;
}

// -------------------------------------------------------------------------
// delete_vertex - Remove a vertex from mesh
// -------------------------------------------------------------------------

bool HandleDeleteVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);

    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
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

    if (!EditMesh.IsVertex(VertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    // Remove the vertex (this will also remove any triangles using it)
    UE::Geometry::EMeshResult RemoveResult = EditMesh.RemoveVertex(VertexIndex);
    bool bSuccess = (RemoveResult == UE::Geometry::EMeshResult::Ok);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetNumberField(TEXT("vertexCount"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex deleted"), Result);
    return true;
}

// -------------------------------------------------------------------------
// delete_triangle - Remove a triangle from mesh
// -------------------------------------------------------------------------

bool HandleGetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);

    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
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

    bool bIsValidVertex = false;
    FVector Position = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexIndex, bIsValidVertex);

    if (!bIsValidVertex)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);

    TSharedPtr<FJsonObject> PosObj = McpHandlerUtils::CreateResultObject();
    PosObj->SetNumberField(TEXT("x"), Position.X);
    PosObj->SetNumberField(TEXT("y"), Position.Y);
    PosObj->SetNumberField(TEXT("z"), Position.Z);
    Result->SetObjectField(TEXT("position"), PosObj);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex position retrieved"), Result);
    return true;
}

// -------------------------------------------------------------------------
// set_vertex_position - Set position of a vertex
// -------------------------------------------------------------------------

bool HandleSetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 VertexIndex = GetIntFieldGeom(Payload, TEXT("vertexIndex"), -1);
    FVector Position = ReadVectorFromPayload(Payload, TEXT("position"), FVector::ZeroVector);

    if (ActorName.IsEmpty() || VertexIndex < 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and vertexIndex required"), TEXT("INVALID_ARGUMENT"));
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
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();

    if (!EditMesh.IsVertex(VertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d"), VertexIndex), TEXT("INVALID_VERTEX"));
        return true;
    }

    EditMesh.SetVertex(VertexIndex, Position);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);

    TSharedPtr<FJsonObject> PosObj = McpHandlerUtils::CreateResultObject();
    PosObj->SetNumberField(TEXT("x"), Position.X);
    PosObj->SetNumberField(TEXT("y"), Position.Y);
    PosObj->SetNumberField(TEXT("z"), Position.Z);
    Result->SetObjectField(TEXT("position"), PosObj);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex position set"), Result);
    return true;
}

// -------------------------------------------------------------------------
// translate_mesh - Translate entire mesh
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
