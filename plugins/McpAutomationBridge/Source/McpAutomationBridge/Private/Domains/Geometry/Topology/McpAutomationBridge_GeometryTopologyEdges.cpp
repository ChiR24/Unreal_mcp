#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT
#include "Operations/MeshPlaneCut.h"

namespace McpGeometryHandlers
{
bool HandleLoopCut(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const int32 NumCuts = FMath::Clamp(GetJsonIntField(Payload, TEXT("numCuts"), 1), 1, 64);
    const int32 AxisIndex = AxisIndexFromPayload(Payload);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const int32 TrisBefore = Mesh->GetTriangleCount();

    // Edge loops where evenly spaced planes cross the mesh. ApplyMeshPlaneCut was
    // used before and it deletes everything on one side of the plane.
    FVector3d PlaneNormal = FVector3d::Zero();
    PlaneNormal[AxisIndex] = 1.0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        const UE::Geometry::FAxisAlignedBox3d Bounds = EditMesh.GetBounds();
        for (int32 Cut = 1; Cut <= NumCuts; ++Cut)
        {
            FVector3d PlaneOrigin = Bounds.Center();
            PlaneOrigin[AxisIndex] = FMath::Lerp(Bounds.Min[AxisIndex], Bounds.Max[AxisIndex], static_cast<double>(Cut) / (NumCuts + 1));
            UE::Geometry::FMeshPlaneCut PlaneCut(&EditMesh, PlaneOrigin, PlaneNormal);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
            PlaneCut.SplitEdgesOnly(false, nullptr);
#else
            PlaneCut.SplitEdgesOnly(false);
#endif
        }
    });
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("numCuts"), NumCuts);
    Result->SetStringField(TEXT("axis"), FString::Chr(TEXT("XYZ")[AxisIndex]));
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loop cut applied"), Result);
    return true;
}

bool HandleEdgeSplit(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
    if (!Payload->TryGetArrayField(TEXT("edges"), Edges) || Edges->Num() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("edges (an array of edge ids) required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const int32 TrisBefore = Mesh->GetTriangleCount();

    // Each valid edge is split at its midpoint (both adjacent triangles split in two).
    int32 EdgesSplit = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Edges)
        {
            UE::Geometry::FDynamicMesh3::FEdgeSplitInfo SplitInfo;
            const int32 EdgeID = Value.IsValid() && Value->Type == EJson::Number ? static_cast<int32>(Value->AsNumber()) : INDEX_NONE;
            if (EditMesh.IsEdge(EdgeID) && EditMesh.SplitEdge(EdgeID, SplitInfo) == UE::Geometry::EMeshResult::Ok)
            {
                ++EdgesSplit;
            }
        }
    });
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgesSplit"), EdgesSplit);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, EdgesSplit > 0,
        EdgesSplit > 0 ? TEXT("Edge split applied") : TEXT("None of the given edge ids exist on the mesh"),
        Result, EdgesSplit > 0 ? FString() : TEXT("INVALID_EDGE"));
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
