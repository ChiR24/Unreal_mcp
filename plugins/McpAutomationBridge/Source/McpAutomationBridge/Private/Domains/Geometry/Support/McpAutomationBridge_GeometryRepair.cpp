#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleWeldVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bMerge)
{
    // merge_vertices is a looser weld that also compacts the mesh.
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double Tolerance = GetJsonNumberField(Payload, TEXT("weldDistance"), bMerge ? 0.001 : 0.0001);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    const int32 VertsBefore = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);

    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = Tolerance;
    WeldOptions.bOnlyUniquePairs = true;
    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    if (bMerge)
    {
        UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);
    }
    const int32 VertsAfter = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("tolerance"), Tolerance);
    Result->SetNumberField(TEXT("verticesBefore"), VertsBefore);
    Result->SetNumberField(TEXT("verticesAfter"), VertsAfter);
    Result->SetNumberField(TEXT("merged"), VertsBefore - VertsAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, bMerge ? TEXT("Vertices merged") : TEXT("Vertices welded"), Result);
    return true;
}

bool HandleFillHoles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::Automatic;

    // UE 5.7: FillAllMeshHoles now takes 5 arguments (added NumFilledHoles and NumFailedHoleFills out params)
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;

    // Filling must only add geometry. On a mesh whose only loops are its real
    // silhouette (e.g. one open triangle) the repair discarded everything, so
    // keep a copy and restore it if any triangle disappears.
    const int32 TriangleCountBefore = Mesh->GetTriangleCount();
    UE::Geometry::FDynamicMesh3 Backup;
    Mesh->ProcessMesh([&Backup](const UE::Geometry::FDynamicMesh3& Source) { Backup = Source; });

    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(
        Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);

    if (Mesh->GetTriangleCount() < TriangleCountBefore)
    {
        Mesh->SetMesh(MoveTemp(Backup));
        DMC->NotifyMeshUpdated();
        TSharedPtr<FJsonObject> Aborted = McpHandlerUtils::CreateResultObject();
        Aborted->SetStringField(TEXT("actorName"), ActorName);
        Aborted->SetNumberField(TEXT("triangleCount"), TriangleCountBefore);
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("fill_holes would have removed existing triangles; the mesh was left unchanged (no fillable hole loops)."),
            Aborted, TEXT("FILL_ABORTED"));
        return true;
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("filledHoles"), NumFilledHoles);
    Result->SetNumberField(TEXT("failedHoles"), NumFailedHoleFills);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Holes filled"), Result);
    return true;
}

bool HandleRemoveDegenerates(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptDegenerateTriangleOptions Options;
    Options.Mode = EGeometryScriptRepairMeshMode::RepairOrDelete;

    UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Degenerate geometry removed"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
