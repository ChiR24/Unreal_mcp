// McpAutomationBridge_GeometryVoxelOperations.cpp — the two grid-based operations: morphology and remesh_voxel.
//
// morphology offsets the surface through a signed-distance grid (dilate and contract move it, close fills the
// creases and fillets the seams where unioned parts meet, open trims small outward features). remesh_voxel
// wraps the mesh in a solid with the engine's voxel wrap. Both rebuild the surface by marching cubes, so
// UVs, material ids, polygroups and vertex colours do not survive and the normals are smooth; the mesh is
// put back when the result is empty or beyond the triangle cap.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "GeometryScript/MeshVoxelFunctions.h"

namespace McpGeometryHandlers
{
// The engine clamps a grid to 256 cells along the longest side, and below 16 the surface is a lump.
static constexpr int32 MinVoxelCount = 16;
static constexpr int32 MaxVoxelCount = 256;
static constexpr int32 DefaultVoxelCount = 128;

static const TCHAR* VoxelAttributeNote()
{
    return TEXT("The surface was rebuilt from a grid: UVs, material ids, polygroups and vertex colours are gone, so run auto_uv, set_material_id and bake_vertex_colors after it.");
}

// The copy to restore, and the mesh's size, read in one pass. False for a mesh with no triangles.
static bool ReadVoxelSource(const FMcpGeometryTarget& Target, UE::Geometry::FDynamicMesh3& OutBackup, double& OutLongest, double& OutArea)
{
    OutLongest = 0.0;
    OutArea = 0.0;
    Target.Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source)
    {
        OutBackup = Source;
        if (Source.TriangleCount() > 0)
        {
            OutLongest = Source.GetBounds().MaxDim();
            for (const int32 TriangleId : Source.TriangleIndicesItr())
            {
                OutArea += Source.GetTriArea(TriangleId);
            }
        }
    });
    return OutBackup.TriangleCount() > 0 && OutLongest > 0.0;
}

// Keeps a voxel operation's result, or puts Backup back and refuses when it is empty or over the triangle cap.
static bool KeepVoxelResult(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                            const FMcpGeometryTarget& Target, UE::Geometry::FDynamicMesh3& Backup, const TCHAR* OpName)
{
    const int32 After = Target.Mesh->GetTriangleCount();
    if (After > 0 && After <= MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        return true;
    }
    Target.Mesh->SetMesh(MoveTemp(Backup));
    Target.Component->NotifyMeshUpdated();
    Self->SendAutomationError(Socket, RequestId,
        After == 0 ? FString::Printf(TEXT("%s left no surface (the distance or voxel size is larger than the thinnest part of the mesh, or the mesh is open); the mesh is unchanged."), OpName)
                   : FString::Printf(TEXT("%s produced %d triangles (max %d); lower voxelCount or raise the distance. The mesh is unchanged."), OpName, After, MAX_TRIANGLES_PER_DYNAMIC_MESH),
        After == 0 ? TEXT("OPERATION_EMPTY") : TEXT("POLYGON_LIMIT_EXCEEDED"));
    return false;
}

bool HandleMorphology(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString Operation = GetJsonStringField(Payload, TEXT("operation"), TEXT("close")).ToLower();
    EGeometryScriptMorphologicalOpType OpType = EGeometryScriptMorphologicalOpType::Close;
    if (Operation == TEXT("dilate")) OpType = EGeometryScriptMorphologicalOpType::Dilate;
    else if (Operation == TEXT("contract")) OpType = EGeometryScriptMorphologicalOpType::Contract;
    else if (Operation == TEXT("open")) OpType = EGeometryScriptMorphologicalOpType::Open;
    else if (Operation != TEXT("close"))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown operation '%s'; use dilate, contract, close or open."), *Operation), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const int32 VoxelCount = FMath::Clamp(GetJsonIntField(Payload, TEXT("voxelCount"), DefaultVoxelCount), MinVoxelCount, MaxVoxelCount);

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    UE::Geometry::FDynamicMesh3 Backup;
    double Longest = 0.0;
    double Area = 0.0;
    if (!ReadVoxelSource(*Target, Backup, Longest, Area))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("The mesh has no triangles to offset."), TEXT("MESH_EMPTY"));
        return true;
    }
    // With no distance, two percent of the mesh's longest side: a visible fillet that does not eat thin parts.
    const double Distance = Payload->HasField(TEXT("distance")) ? GetJsonNumberField(Payload, TEXT("distance"), 0.0) : Longest * 0.02;
    if (!FMath::IsFinite(Distance) || Distance <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("distance must be a positive number of centimetres."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, 0, TEXT("Morphology"))) return true;

    FGeometryScriptMorphologyOptions Options;
    Options.SDFGridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridResolution;
    Options.SDFGridParameters.GridResolution = VoxelCount;
    Options.Operation = OpType;
    Options.Distance = static_cast<float>(Distance);
    UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshMorphology(Target->Mesh, Options, nullptr);
    if (!KeepVoxelResult(Self, RequestId, Socket, *Target, Backup, TEXT("Morphology"))) return true;
    Target->Component->NotifyMeshUpdated();

    // The grid spans the mesh plus the distance on both sides, so a voxel is this wide.
    const double VoxelSize = (Longest + Distance * 2.0) / VoxelCount;
    FString Note = VoxelAttributeNote();
    if (Distance < VoxelSize)
    {
        Note += FString::Printf(TEXT(" distance %.2f is under one voxel (%.2f cm), so the offset barely registers: raise voxelCount or distance."), Distance, VoxelSize);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), Operation);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("voxelCount"), VoxelCount);
    Result->SetNumberField(TEXT("voxelSize"), VoxelSize);
    Result->SetNumberField(TEXT("trianglesBefore"), Backup.TriangleCount());
    Result->SetNumberField(TEXT("trianglesAfter"), Target->Mesh->GetTriangleCount());
    Result->SetStringField(TEXT("note"), Note);
    McpHandlerUtils::AddVerification(Result, Target->Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Morphology %s applied"), *Operation), Result);
    return true;
}

bool HandleRemeshVoxel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    UE::Geometry::FDynamicMesh3 Backup;
    double Longest = 0.0;
    double Area = 0.0;
    if (!ReadVoxelSource(*Target, Backup, Longest, Area))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("The mesh has no triangles to remesh."), TEXT("MESH_EMPTY"));
        return true;
    }

    // One grid cell is the target edge length; a voxel count or a triangle budget is turned into one.
    double CellSize = Longest / DefaultVoxelCount;
    if (Payload->HasField(TEXT("targetEdgeLength")))
    {
        CellSize = GetJsonNumberField(Payload, TEXT("targetEdgeLength"), CellSize);
    }
    else if (Payload->HasField(TEXT("voxelCount")))
    {
        CellSize = Longest / FMath::Clamp(GetJsonIntField(Payload, TEXT("voxelCount"), DefaultVoxelCount), MinVoxelCount, MaxVoxelCount);
    }
    else if (Payload->HasField(TEXT("targetTriangleCount")))
    {
        // A surface wrapped in cells of side c comes out at about 2 * area / c^2 triangles.
        CellSize = FMath::Sqrt(2.0 * Area / FMath::Max(100, GetJsonIntField(Payload, TEXT("targetTriangleCount"), 5000)));
    }
    if (!FMath::IsFinite(CellSize) || CellSize <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetEdgeLength must be a positive number of centimetres."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    CellSize = FMath::Max(CellSize, Longest / MaxVoxelCount);
    if (!GuardMeshBudget(Self, RequestId, Socket, 0, TEXT("Voxel remesh"))) return true;

    FGeometryScriptSolidifyOptions Options;
    Options.GridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridCellSize;
    Options.GridParameters.GridCellSize = static_cast<float>(CellSize);
    Options.ExtendBounds = static_cast<float>(CellSize * 2.0);
    // An open shell has no inside to wrap, so thicken it to a cell or two first; closed parts are left alone.
    Options.bThickenShells = true;
    Options.ShellThickness = CellSize;
    UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(Target->Mesh, Options, nullptr);
    if (!KeepVoxelResult(Self, RequestId, Socket, *Target, Backup, TEXT("Voxel remesh"))) return true;
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("voxelSize"), CellSize);
    Result->SetNumberField(TEXT("trianglesBefore"), Backup.TriangleCount());
    Result->SetNumberField(TEXT("trianglesAfter"), Target->Mesh->GetTriangleCount());
    Result->SetStringField(TEXT("note"), VoxelAttributeNote());
    McpHandlerUtils::AddVerification(Result, Target->Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Voxel remesh applied"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
