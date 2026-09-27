#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGetMeshInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    UDynamicMesh* Mesh = Target->Mesh;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexCount"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetBoolField(TEXT("hasNormals"), UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleNormals(Mesh));
    Result->SetBoolField(TEXT("hasUVs"), UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets(Mesh) > 0);
    Result->SetBoolField(TEXT("hasColors"), UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexColors(Mesh));
    Result->SetBoolField(TEXT("hasPolygroups"), UGeometryScriptLibrary_MeshQueryFunctions::GetHasMaterialIDs(Mesh));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh info retrieved"), Result);
    return true;
}

bool HandleRecalculateNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const bool bAreaWeighted = GetJsonBoolField(Payload, TEXT("computeWeightedNormals"), true);
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    FGeometryScriptCalculateNormalsOptions NormalOptions;
    NormalOptions.bAreaWeighted = bAreaWeighted;
    NormalOptions.bAngleWeighted = true;
    if (Payload->HasField(TEXT("hardEdgeAngle")))
    {
        // Creases sharper than the angle keep hard (split) normals.
        FGeometryScriptSplitNormalsOptions SplitOptions;
        SplitOptions.bSplitByOpeningAngle = true;
        SplitOptions.OpeningAngleDeg = GetJsonNumberField(Payload, TEXT("hardEdgeAngle"), 60.0);
        SplitOptions.bSplitByFaceGroup = false;
        UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Target->Mesh, SplitOptions, NormalOptions, nullptr);
    }
    else
    {
        RecomputeMeshNormals(Target->Mesh, NormalOptions);
    }
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("areaWeighted"), bAreaWeighted);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals recalculated"), Result);
    return true;
}

bool HandleFlipNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Target->Mesh, nullptr);
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals flipped"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
