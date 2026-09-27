#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAppendVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    const int32 VertexIndex = Target->Mesh->GetMeshRef().AppendVertex(UE::Geometry::FVertexInfo(Position));
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetNumberField(TEXT("vertexCount"), Target->Mesh->GetMeshRef().VertexCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex appended"), Result);
    return true;
}

// get/set_vertex_position: the target mesh and a vertex id that exists on it; unset after replying otherwise.
static TOptional<FMcpGeometryTarget> ResolveVertex(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                                   const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                                   int32& OutVertexIndex)
{
    OutVertexIndex = GetJsonIntField(Payload, TEXT("vertexIndex"), -1);
    TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, GetJsonStringField(Payload, TEXT("actorName")), Socket);
    if (Target && !Target->Mesh->GetMeshRef().IsVertex(OutVertexIndex))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid vertex index: %d (the dynamic mesh has %d vertices; build or import geometry first)"),
                            OutVertexIndex, Target->Mesh->GetMeshRef().VertexCount()), TEXT("INVALID_VERTEX"));
        return {};
    }
    return Target;
}

static bool SendVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                               const TSharedPtr<FJsonObject>& Payload, int32 VertexIndex, const FVector& Position, const TCHAR* Message)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), GetJsonStringField(Payload, TEXT("actorName")));
    Result->SetNumberField(TEXT("vertexIndex"), VertexIndex);
    Result->SetObjectField(TEXT("position"), McpHandlerUtils::VectorToJson(Position));
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}

bool HandleGetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 VertexIndex = INDEX_NONE;
    const TOptional<FMcpGeometryTarget> Target = ResolveVertex(Self, RequestId, Payload, Socket, VertexIndex);
    if (!Target) return true;
    return SendVertexPosition(Self, RequestId, Socket, Payload, VertexIndex,
                              FVector(Target->Mesh->GetMeshRef().GetVertex(VertexIndex)), TEXT("Vertex position retrieved"));
}

bool HandleSetVertexPosition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 VertexIndex = INDEX_NONE;
    const TOptional<FMcpGeometryTarget> Target = ResolveVertex(Self, RequestId, Payload, Socket, VertexIndex);
    if (!Target) return true;
    const FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);
    Target->Mesh->GetMeshRef().SetVertex(VertexIndex, Position);
    Target->Component->NotifyMeshUpdated();
    return SendVertexPosition(Self, RequestId, Socket, Payload, VertexIndex, Position, TEXT("Vertex position set"));
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
