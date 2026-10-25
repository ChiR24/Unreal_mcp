// McpGeometryMatIds.cpp — set_material_id: which material slot each triangle belongs to; delete_triangles.
//
// A triangle's material id becomes its static-mesh material slot when the mesh is baked by convert_to_static_mesh
// or convert_to_nanite, so ids are how one mesh ends up with several materials. The triangles are named by
// triangleIndices or by a region (see McpGeometryRegion.cpp); with neither, every triangle is set. delete_triangles
// takes the same selection and removes what it picks.
#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleSetMaterialId(UEngineWireSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double IdValue = 0.0;
    if (!Payload->TryGetNumberField(TEXT("materialId"), IdValue))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("materialId required: the material slot (0 or more) the triangles are assigned to"), TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (!FMath::IsFinite(IdValue) || IdValue != FMath::FloorToDouble(IdValue) || IdValue < 0.0 || IdValue > MAX_MATERIAL_ID)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("materialId must be a whole number from 0 to %d"), MAX_MATERIAL_ID), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const int32 MaterialId = static_cast<int32>(IdValue);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    TArray<int32> Selected;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection, &Selected)) return true;

    int32 Assigned = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        EditMesh.EnableAttributes();
        if (!EditMesh.Attributes()->HasMaterialID())
        {
            EditMesh.Attributes()->EnableMaterialID();
        }
        UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = EditMesh.Attributes()->GetMaterialID();
        if (bHasSelection)
        {
            for (const int32 TriangleId : Selected)
            {
                if (EditMesh.IsTriangle(TriangleId))
                {
                    MaterialIds->SetValue(TriangleId, MaterialId);
                    ++Assigned;
                }
            }
        }
        else
        {
            for (const int32 TriangleId : EditMesh.TriangleIndicesItr())
            {
                MaterialIds->SetValue(TriangleId, MaterialId);
                ++Assigned;
            }
        }
    });
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("materialId"), MaterialId);
    Result->SetNumberField(TEXT("trianglesSelected"), Assigned);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetArrayField(TEXT("materialIdsInUse"), MaterialIdsInUseJson(Mesh));
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Material id %d set on %d triangles"), MaterialId, Assigned), Result);
    return true;
}

// Every id the mesh carries now, so the next set_material_id or the conversion's materials list lines up.
TArray<TSharedPtr<FJsonValue>> MaterialIdsInUseJson(UDynamicMesh* Mesh)
{
    TArray<int32> IdsInUse;
    Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
    {
        TSet<int32> Unique;
        const UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds =
            ReadMesh.HasAttributes() ? ReadMesh.Attributes()->GetMaterialID() : nullptr;
        for (const int32 TriangleId : ReadMesh.TriangleIndicesItr())
        {
            Unique.Add(MaterialIds ? MaterialIds->GetValue(TriangleId) : 0);
        }
        IdsInUse = Unique.Array();
        IdsInUse.Sort();
    });
    TArray<TSharedPtr<FJsonValue>> InUseJson;
    for (const int32 Id : IdsInUse)
    {
        InUseJson.Add(MakeShared<FJsonValueNumber>(Id));
    }
    return InUseJson;
}

// delete_triangles: the triangles triangleIndices or a region pick are removed, with the points only they used. A
// part built too big, a face nobody sees, or a cuboid that should become something else had no way out of a mesh.
bool HandleDeleteTriangles(UEngineWireSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    FGeometryScriptMeshSelection Selection;
    bool bHasSelection = false;
    TArray<int32> Selected;
    if (!ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection, &Selected)) return true;
    if (!bHasSelection)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("give triangleIndices or a region: delete_triangles removes the triangles they pick and never the whole mesh"), TEXT("INVALID_SELECTION"));
        return true;
    }
    int32 Removed = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        for (const int32 TriangleId : Selected)
        {
            if (EditMesh.IsTriangle(TriangleId) && EditMesh.RemoveTriangle(TriangleId, true, false) == UE::Geometry::EMeshResult::Ok)
            {
                ++Removed;
            }
        }
    });
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesRemoved"), Removed);
    Result->SetNumberField(TEXT("vertexCount"), Mesh->GetMeshRef().VertexCount());
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetArrayField(TEXT("materialIdsInUse"), MaterialIdsInUseJson(Mesh));
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Removed %d triangles"), Removed), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
