#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Changes/MeshReplacementChange.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/ITransaction.h"

namespace McpGeometryHandlers
{
FMcpGeometryUndoScope::FMcpGeometryUndoScope(const TSharedPtr<FJsonObject>& Payload)
{
    // Only an actor that is there before the request: a create makes its own, and spawning is its own undo step.
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    ADynamicMeshActor* Actor = ActorName.IsEmpty() ? nullptr : FindGeometryActor<ADynamicMeshActor>(ActorName);
    UDynamicMeshComponent* Target = Actor ? Actor->GetDynamicMeshComponent() : nullptr;
    if (!Target || !Target->GetDynamicMesh())
    {
        return;
    }
    Component = Target;
    Target->GetDynamicMesh()->ProcessMesh([this](const UE::Geometry::FDynamicMesh3& Mesh)
    {
        Before = MakeShared<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>(Mesh);
    });
}

FMcpGeometryUndoScope::~FMcpGeometryUndoScope()
{
    UDynamicMeshComponent* Target = Component.Get();
    if (!Target || !Target->GetDynamicMesh() || !Before.IsValid())
    {
        return;
    }
    TSharedPtr<const UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> After;
    Target->GetDynamicMesh()->ProcessMesh([this, &After](const UE::Geometry::FDynamicMesh3& Mesh)
    {
        // Every part a request can change: positions, triangles, polygroups and each attribute layer (UVs, normals,
        // colours, material ids). Comparing positions, triangles, UVs and normals alone missed set_material_id and
        // polygroup edits: they recorded no step, and the next undo rolled back the step before them, edit and all.
        UE::Geometry::FDynamicMesh3::FSameAsOptions Same;
        Same.bCheckNormals = Same.bCheckColors = Same.bCheckUVs = Same.bCheckGroups = Same.bCheckAttributes = true;
        if (!Mesh.IsSameAs(*Before, Same))
        {
            After = MakeShared<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>(Mesh);
        }
    });
    if (!After.IsValid())
    {
        return;
    }
    // The change holds both meshes, so it can be stored after the edit; undo applies Before through the component.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit Dynamic Mesh")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    if (GUndo)
    {
        GUndo->StoreUndo(Target, MakeUnique<FMeshReplacementChange>(Before, After));
    }
}
}

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
