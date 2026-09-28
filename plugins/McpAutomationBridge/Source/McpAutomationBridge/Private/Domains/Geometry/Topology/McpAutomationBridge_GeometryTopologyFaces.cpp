#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "Polygroups/PolygroupsGenerator.h"

// poke: a centre vertex per triangle and a fan of three triangles around it.
// quadrangulate: pairs of triangles become quad PolyGroups; the mesh itself stays triangles.
namespace McpGeometryHandlers
{
bool HandlePoke(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const double Offset = GetJsonNumberField(Payload, TEXT("distance"), 0.0);
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;
    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection)) return true;
    const int32 TrisBefore = Mesh->GetTriangleCount();
    if (!GuardMeshBudget(Self, RequestId, Socket, static_cast<int64>(TrisBefore) * 3, TEXT("Poke"))) return true;

    // The ids are taken before the edit: poking adds triangles, and only the original ones are poked.
    TArray<int32> Requested;
    const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
    if (bHasSelection && Payload->TryGetArrayField(TEXT("triangleIndices"), Indices))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Indices)
        {
            Requested.AddUnique(static_cast<int32>(Value->AsNumber()));
        }
    }
    const int32 VertsBefore = Mesh->GetMeshRef().VertexCount();
    int32 Poked = 0;
    int32 Skipped = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        TArray<int32> TriangleIds;
        if (bHasSelection)
        {
            TriangleIds = Requested;
        }
        else
        {
            for (int32 TID : EditMesh.TriangleIndicesItr()) TriangleIds.Add(TID);
        }
        for (const int32 TID : TriangleIds)
        {
            if (!EditMesh.IsTriangle(TID)) { ++Skipped; continue; }
            const FVector3d Normal = EditMesh.GetTriNormal(TID);
            UE::Geometry::FDynamicMesh3::FPokeTriangleInfo Info;
            if (EditMesh.PokeTriangle(TID, Info) != UE::Geometry::EMeshResult::Ok) { ++Skipped; continue; }
            if (Offset != 0.0)
            {
                EditMesh.SetVertex(Info.NewVertex, EditMesh.GetVertex(Info.NewVertex) + Normal * Offset);
            }
            ++Poked;
        }
    });
    if (Poked > 0)
    {
        RecomputeMeshNormals(Mesh);
    }
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesPoked"), Poked);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("verticesAdded"), Mesh->GetMeshRef().VertexCount() - VertsBefore);
    Result->SetNumberField(TEXT("skipped"), Skipped);
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, Poked > 0,
        Poked > 0 ? FString::Printf(TEXT("Poked %d triangles"), Poked) : FString(TEXT("No triangle could be poked (all were degenerate)")),
        Result, Poked > 0 ? FString() : TEXT("NOTHING_POKED"));
    return true;
}

bool HandleQuadrangulate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const bool bRespectUVSeams = GetJsonBoolField(Payload, TEXT("respectUVSeams"), true);
    const bool bRespectHardNormals = GetJsonBoolField(Payload, TEXT("respectHardNormals"), false);
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    int32 Quads = 0;
    int32 Singles = 0;
    int32 Larger = 0;
    bool bFound = false;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        UE::Geometry::FPolygroupsGenerator Generator(&EditMesh);
        bFound = Generator.FindSourceMeshPolygonPolygroups(bRespectUVSeams, bRespectHardNormals, 1.0, 1.0, 1);
        if (!bFound) return;
        for (const TArray<int>& Group : Generator.FoundPolygroups)
        {
            Quads += Group.Num() == 2 ? 1 : 0;
            Singles += Group.Num() == 1 ? 1 : 0;
            Larger += Group.Num() > 2 ? 1 : 0;
        }
        // With no pair found the existing PolyGroups are kept rather than replaced by one group per triangle.
        if (Quads > 0)
        {
            Generator.CopyPolygroupsToMesh();
        }
    });
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("quadsFormed"), Quads);
    Result->SetNumberField(TEXT("singleTriangleGroups"), Singles);
    Result->SetNumberField(TEXT("largerGroups"), Larger);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetStringField(TEXT("note"), TEXT("The mesh stays triangles; each quad is a PolyGroup of two triangles that PolyGroup edits treat as one face."));
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, bFound && Quads > 0,
        bFound && Quads > 0 ? FString::Printf(TEXT("Grouped %d triangle pairs into quad PolyGroups"), Quads)
                            : FString(TEXT("No two triangles could be paired into a quad")),
        Result, bFound && Quads > 0 ? FString() : TEXT("NO_QUADS"));
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
