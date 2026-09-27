#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool GuardMeshBudget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                     int64 EstimatedTriangles, const TCHAR* OpName)
{
    // Starting a heavy mesh op on a starved system can take the editor (and unsaved work) down.
    const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    const double UsedFraction = static_cast<double>(MemStats.UsedPhysical) / static_cast<double>(MemStats.TotalPhysical);
    if (UsedFraction >= MEMORY_PRESSURE_CRITICAL)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used); %s blocked to prevent OOM."), UsedFraction * 100.0, OpName),
            TEXT("MEMORY_PRESSURE"));
        return false;
    }
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s would produce ~%lld triangles (max %d)."), OpName, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return false;
    }
    return true;
}

int32 DeformVertices(UDynamicMesh* Mesh, TFunctionRef<FVector(const FVector&)> Move)
{
    int32 Moved = 0;
    Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            const FVector Before(EditMesh.GetVertex(VID));
            const FVector After = Move(Before);
            if (!After.Equals(Before, 0.0))
            {
                EditMesh.SetVertex(VID, FVector3d(After));
                ++Moved;
            }
        }
    });
    return Moved;
}

void RecomputeMeshNormals(UDynamicMesh* Mesh, const FGeometryScriptCalculateNormalsOptions& Options)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, Options, false, nullptr);
#else
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, Options, nullptr);
#endif
}

int32 ClampSegments(int32 Value, int32 Default)
{
    return FMath::Clamp(Value <= 0 ? Default : Value, 1, MAX_SEGMENTS);
}

int32 DeclaredSegments(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> Names, int32 Default)
{
    for (const TCHAR* Name : Names)
    {
        if (Payload.IsValid() && Payload->HasField(Name))
        {
            return ClampSegments(GetJsonIntField(Payload, Name, Default), Default);
        }
    }
    return ClampSegments(Default, Default);
}

double ClampDimension(double Value, double Default)
{
    if (Value <= 0.0) Value = Default;
    return FMath::Clamp(Value, MIN_DIMENSION, MAX_DIMENSION);
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
