#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Changes/MeshReplacementChange.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/ITransaction.h"

namespace McpGeometryHandlers
{
namespace
{
// Positions, triangles and the primary UV and normal layers: what a geometry edit changes. Change stamps would be
// cheaper, but from 5.7 they are off by default and reading one fires an ensure.
uint32 MeshFingerprint(const UE::Geometry::FDynamicMesh3& Mesh)
{
    uint32 Hash = HashCombine(GetTypeHash(Mesh.VertexCount()), GetTypeHash(Mesh.TriangleCount()));
    for (const int32 Vertex : Mesh.VertexIndicesItr())
    {
        Hash = HashCombine(Hash, GetTypeHash(Mesh.GetVertex(Vertex)));
    }
    for (const int32 Triangle : Mesh.TriangleIndicesItr())
    {
        const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(Triangle);
        Hash = HashCombine(HashCombine(HashCombine(Hash, GetTypeHash(Tri.A)), GetTypeHash(Tri.B)), GetTypeHash(Tri.C));
    }
    if (const UE::Geometry::FDynamicMeshAttributeSet* Attributes = Mesh.Attributes())
    {
        if (const UE::Geometry::FDynamicMeshUVOverlay* UVs = Attributes->PrimaryUV())
        {
            for (const int32 Element : UVs->ElementIndicesItr())
            {
                Hash = HashCombine(Hash, GetTypeHash(UVs->GetElement(Element)));
            }
        }
        if (const UE::Geometry::FDynamicMeshNormalOverlay* Normals = Attributes->PrimaryNormals())
        {
            for (const int32 Element : Normals->ElementIndicesItr())
            {
                Hash = HashCombine(Hash, GetTypeHash(Normals->GetElement(Element)));
            }
        }
    }
    return Hash;
}
}

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
        BeforeHash = MeshFingerprint(Mesh);
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
        if (MeshFingerprint(Mesh) != BeforeHash)
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
