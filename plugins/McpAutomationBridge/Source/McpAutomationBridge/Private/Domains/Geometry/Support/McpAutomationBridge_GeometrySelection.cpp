// McpAutomationBridge_GeometrySelection.cpp — triangle selections for the face operators.
//
// Dogfood #137: extrude/inset/outset/offset_faces/bevel applied to the whole mesh
// because every handler passed an empty FGeometryScriptMeshSelection. An optional
// `triangleIndices` array now limits the operation to those triangles, and an optional
// `region` (see McpAutomationBridge_GeometryRegion.cpp) picks them by where they are.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "GeometryScript/MeshSelectionFunctions.h"
#include "UDynamicMesh.h"

namespace McpGeometryHandlers
{
// The triangle ids a payload names, by triangleIndices or by region; bOutHasSelection stays false when it names none.
static bool BuildTriangleSelection(UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload,
                                   FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection,
                                   TArray<int32>* OutTriangleIds, FString& OutError, FString& OutCode)
{
    bOutHasSelection = false;
    OutCode = TEXT("INVALID_SELECTION");
    const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
    const bool bHasIndices = Payload->TryGetArrayField(TEXT("triangleIndices"), Indices) && Indices->Num() > 0;
    const TSharedPtr<FJsonObject>* Region = nullptr;
    const bool bHasRegion = Payload->TryGetObjectField(TEXT("region"), Region) && Region->IsValid();
    if (bHasIndices && bHasRegion)
    {
        OutError = TEXT("give either triangleIndices or region to select triangles, not both");
        return false;
    }
    if (!bHasIndices && !bHasRegion)
    {
        return true; // no selection requested: the operator applies to the whole mesh
    }

    TArray<int32> TriangleIds;
    if (bHasRegion)
    {
        if (!ResolveRegionTriangles(Mesh, *Region, TriangleIds, OutError, OutCode)) return false;
    }
    else
    {
        TSet<int32> Seen;
        TriangleIds.Reserve(Indices->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Indices)
        {
            if (!Value.IsValid() || Value->Type != EJson::Number || Value->AsNumber() < 0)
            {
                OutError = TEXT("triangleIndices must be non-negative integer triangle ids");
                return false;
            }
            const int32 TriangleId = static_cast<int32>(Value->AsNumber());
            if (!Seen.Contains(TriangleId))
            {
                Seen.Add(TriangleId);
                TriangleIds.Add(TriangleId);
            }
        }
    }
    UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(
        Mesh, TriangleIds, EGeometryScriptMeshSelectionType::Triangles, OutSelection);
    if (OutSelection.GetNumSelected() == 0)
    {
        OutError = FString::Printf(TEXT("none of the %d triangleIndices exist on the mesh"), TriangleIds.Num());
        return false;
    }
    bOutHasSelection = true;
    if (OutTriangleIds)
    {
        *OutTriangleIds = MoveTemp(TriangleIds);
    }
    return true;
}

bool ReadTriangleSelection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Payload,
                           FGeometryScriptMeshSelection& OutSelection, bool& bOutHasSelection, TArray<int32>* OutTriangleIds)
{
    FString Error;
    FString Code;
    if (BuildTriangleSelection(Mesh, Payload, OutSelection, bOutHasSelection, OutTriangleIds, Error, Code)) return true;
    Self->SendAutomationError(Socket, RequestId, Error, Code);
    return false;
}

int32 SelectedTriangleCount(UDynamicMesh* Mesh, const FGeometryScriptMeshSelection& Selection, bool bHasSelection)
{
    return bHasSelection ? Selection.GetNumSelected() : Mesh->GetTriangleCount();
}

double FaceOpDistance(const TSharedPtr<FJsonObject>& Payload, double Default)
{
    return GetJsonNumberField(Payload, TEXT("distance"), GetJsonNumberField(Payload, TEXT("amount"), Default));
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
