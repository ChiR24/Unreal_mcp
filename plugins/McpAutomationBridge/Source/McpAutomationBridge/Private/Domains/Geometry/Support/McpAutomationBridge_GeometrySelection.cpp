// McpAutomationBridge_GeometrySelection.cpp — triangle selections for the face operators.
//
// Dogfood #137: extrude/inset/outset/offset_faces/bevel/chamfer applied to the whole mesh
// because every handler passed an empty FGeometryScriptMeshSelection. An optional
// `triangleIndices` (alias `faceIndices`) array now limits the operation to those triangles.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#include "GeometryScript/MeshSelectionFunctions.h"
#include "UDynamicMesh.h"

namespace McpGeometryHandlers
{
bool McpBuildTriangleSelection(UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload,
                               FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection,
                               FString& OutError)
{
    bOutHasSelection = false;
    OutError.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
    if (!Payload.IsValid() ||
        (!Payload->TryGetArrayField(TEXT("triangleIndices"), Indices) &&
         !Payload->TryGetArrayField(TEXT("faceIndices"), Indices)) ||
        !Indices || Indices->Num() == 0)
    {
        return true; // no selection requested: the operator applies to the whole mesh
    }
    if (!Mesh)
    {
        OutError = TEXT("triangleIndices given but the dynamic mesh is unavailable");
        return false;
    }
    TArray<int32> TriangleIds;
    TriangleIds.Reserve(Indices->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Indices)
    {
        if (!Value.IsValid() || Value->Type != EJson::Number || Value->AsNumber() < 0)
        {
            OutError = TEXT("triangleIndices must be non-negative integer triangle ids");
            return false;
        }
        TriangleIds.Add(static_cast<int32>(Value->AsNumber()));
    }
    UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(
        Mesh, TriangleIds, EGeometryScriptMeshSelectionType::Triangles, OutSelection);
    if (OutSelection.GetNumSelected() == 0)
    {
        OutError = FString::Printf(TEXT("none of the %d triangleIndices exist on the mesh"), TriangleIds.Num());
        return false;
    }
    bOutHasSelection = true;
    return true;
}
} // namespace McpGeometryHandlers
