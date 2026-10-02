// McpAutomationBridge_GeometryAppendPolygons.cpp — append_polygons: author a whole polygon cage in one call.
//
// The input is validated first (McpAutomationBridge_GeometryPolygonInput.cpp), so a refusal leaves the mesh as it
// was. The points and triangles are then appended in one edit, each face's triangles in the face's polygroup,
// with its material id when faceMaterials is given.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleAppendPolygons(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FAppendPolygonsInput Input;
    FString Error;
    if (!ParseAppendPolygonsInput(Payload, Input, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_POLYGONS"));
        return true;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, static_cast<int64>(Mesh->GetTriangleCount()) + Input.Triangles.Num(), TEXT("Append polygons"))) return true;

    // A copy to fall back on: the edit below must leave the mesh as it was if the engine refuses a triangle.
    UE::Geometry::FDynamicMesh3 Backup;
    Mesh->ProcessMesh([&Backup](const UE::Geometry::FDynamicMesh3& Source) { Backup = Source; });
    bool bAppended = true;
    int32 VerticesAdded = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        if (!EditMesh.HasTriangleGroups())
        {
            EditMesh.EnableTriangleGroups();
        }
        UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = nullptr;
        if (Input.FaceMaterials.Num() > 0)
        {
            EditMesh.EnableAttributes();
            if (!EditMesh.Attributes()->HasMaterialID())
            {
                EditMesh.Attributes()->EnableMaterialID();
            }
            MaterialIds = EditMesh.Attributes()->GetMaterialID();
        }
        // A point no face uses is left out: an isolated vertex would stretch the mesh's bounds, which masks and offsets read.
        TArray<bool> Used;
        Used.Init(false, Input.Vertices.Num());
        for (const UE::Geometry::FIndex3i& Triangle : Input.Triangles)
        {
            Used[Triangle.A] = Used[Triangle.B] = Used[Triangle.C] = true;
        }
        TArray<int32> VertexIds;
        VertexIds.Init(INDEX_NONE, Input.Vertices.Num());
        for (int32 VertexIndex = 0; VertexIndex < Input.Vertices.Num(); ++VertexIndex)
        {
            if (Used[VertexIndex])
            {
                VertexIds[VertexIndex] = EditMesh.AppendVertex(UE::Geometry::FVertexInfo(Input.Vertices[VertexIndex]));
                ++VerticesAdded;
            }
        }
        TArray<int32> GroupOfFace;
        GroupOfFace.Reserve(Input.Faces.Num());
        for (int32 FaceIndex = 0; FaceIndex < Input.Faces.Num(); ++FaceIndex)
        {
            GroupOfFace.Add(Input.FaceGroups.Num() > 0 ? Input.FaceGroups[FaceIndex] : EditMesh.AllocateTriangleGroup());
        }
        for (int32 TriangleIndex = 0; TriangleIndex < Input.Triangles.Num() && bAppended; ++TriangleIndex)
        {
            const UE::Geometry::FIndex3i& Triangle = Input.Triangles[TriangleIndex];
            const int32 Face = Input.TriangleFace[TriangleIndex];
            const int32 TriangleId = EditMesh.AppendTriangle(VertexIds[Triangle.A], VertexIds[Triangle.B], VertexIds[Triangle.C], GroupOfFace[Face]);
            if (TriangleId < 0)
            {
                bAppended = false;
                Error = FString::Printf(TEXT("faces[%d] was refused by the mesh (code %d): it would duplicate a face or make an edge non-manifold"), Face, TriangleId);
            }
            else if (MaterialIds)
            {
                MaterialIds->SetValue(TriangleId, Input.FaceMaterials[Face]);
            }
        }
    });
    if (!bAppended)
    {
        Mesh->SetMesh(MoveTemp(Backup));
        DMC->NotifyMeshUpdated();
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_POLYGONS"));
        return true;
    }
    RecomputeMeshNormals(Mesh);
    DMC->NotifyMeshUpdated();

    int32 GroupCount = 0;
    Mesh->ProcessMesh([&GroupCount](const UE::Geometry::FDynamicMesh3& ReadMesh)
    {
        TSet<int32> Groups;
        for (const int32 TriangleId : ReadMesh.TriangleIndicesItr())
        {
            Groups.Add(ReadMesh.GetTriangleGroup(TriangleId));
        }
        GroupCount = Groups.Num();
    });
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("verticesAdded"), VerticesAdded);
    Result->SetNumberField(TEXT("facesAdded"), Input.Faces.Num());
    Result->SetNumberField(TEXT("trianglesAdded"), Input.Triangles.Num());
    Result->SetNumberField(TEXT("vertexCount"), Mesh->GetMeshRef().VertexCount());
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("groupCount"), GroupCount);
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Appended %d faces (%d triangles)"), Input.Faces.Num(), Input.Triangles.Num()), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
