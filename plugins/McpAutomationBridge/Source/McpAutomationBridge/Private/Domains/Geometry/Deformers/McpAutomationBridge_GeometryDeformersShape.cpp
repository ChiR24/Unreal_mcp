#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSpherify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double Factor = GetJsonNumberField(Payload, TEXT("strength"), 1.0);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    const FVector Center = BBox.GetCenter();
    const double TargetRadius = BBox.GetExtent().GetMax();
    const double Alpha = FMath::Clamp(Factor, 0.0, 1.0);
    DeformVertices(Mesh, [&](const FVector& Pos)
    {
        const FVector Direction = Pos - Center;
        return Direction.Size() > KINDA_SMALL_NUMBER ? FMath::Lerp(Pos, Center + Direction.GetUnsafeNormal() * TargetRadius, Alpha) : Pos;
    });
    RecomputeMeshNormals(Mesh);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spherify applied"), Result);
    return true;
}

bool HandleCylindrify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double Factor = GetJsonNumberField(Payload, TEXT("strength"), 1.0);
    const int32 AxisIndex = AxisIndexFromPayload(Payload);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    const FVector Center = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh).GetCenter();
    // Average distance from the cylinder axis becomes the cylinder radius.
    double TotalRadius = 0.0;
    int32 VertexCount = 0;
    const UE::Geometry::FDynamicMesh3& ReadMesh = Mesh->GetMeshRef();
    for (int32 VID : ReadMesh.VertexIndicesItr())
    {
        FVector Perpendicular = FVector(ReadMesh.GetVertex(VID)) - Center;
        Perpendicular[AxisIndex] = 0.0;
        TotalRadius += Perpendicular.Size();
        ++VertexCount;
    }
    double AvgRadius = VertexCount > 0 ? TotalRadius / VertexCount : 1.0;
    if (AvgRadius < KINDA_SMALL_NUMBER) AvgRadius = 1.0;

    const double Alpha = FMath::Clamp(Factor, 0.0, 1.0);
    const int32 VerticesModified = DeformVertices(Mesh, [&](const FVector& Pos)
    {
        FVector Perpendicular = Pos - Center;
        Perpendicular[AxisIndex] = 0.0;
        if (Perpendicular.Size() <= KINDA_SMALL_NUMBER) return Pos;
        FVector CylinderPos = Center + Perpendicular.GetUnsafeNormal() * AvgRadius;
        CylinderPos[AxisIndex] = Pos[AxisIndex];
        return FMath::Lerp(Pos, CylinderPos, Alpha);
    });
    RecomputeMeshNormals(Mesh);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper());
    Result->SetNumberField(TEXT("factor"), Factor);
    Result->SetNumberField(TEXT("avgRadius"), AvgRadius);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylindrify applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
