// McpAutomationBridge_GeometrySelection.cpp — triangle selections for the face operators.
//
// Dogfood #137: extrude/inset/outset/offset_faces/bevel applied to the whole mesh
// because every handler passed an empty FGeometryScriptMeshSelection. An optional
// `triangleIndices` array now limits the operation to those triangles.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "GeometryScript/MeshSelectionFunctions.h"
#include "UDynamicMesh.h"

namespace McpGeometryHandlers
{
static bool BuildTriangleSelection(UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload,
                                   FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection, FString& OutError)
{
    bOutHasSelection = false;
    const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
    if (!Payload->TryGetArrayField(TEXT("triangleIndices"), Indices) || Indices->Num() == 0)
    {
        return true; // no selection requested: the operator applies to the whole mesh
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

bool ReadTriangleSelection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload,
                           FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection)
{
    FString Error;
    if (BuildTriangleSelection(Mesh, Payload, OutSelection, bOutHasSelection, Error)) return true;
    Self->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SELECTION"));
    return false;
}

double FaceOpDistance(const TSharedPtr<FJsonObject>& Payload, double Default)
{
    return GetJsonNumberField(Payload, TEXT("distance"), GetJsonNumberField(Payload, TEXT("amount"), Default));
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
