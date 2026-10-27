// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

namespace McpCoplanar
{
FMcpCoplanarFill MeasureFill(const UStaticMesh* Mesh)
{
    FMcpCoplanarFill Fill;
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() == 0)
    {
        return Fill;
    }
    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    const FPositionVertexBuffer& Positions = Lod.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& Tangents = Lod.VertexBuffers.StaticMeshVertexBuffer;
    const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
    // Render data kept without a CPU copy cannot be read; that mesh is simply not judged.
    if (!Positions.GetVertexData() || !Tangents.GetTangentData() || Indices.Num() < 3)
    {
        return Fill;
    }
    const FBox Box = Mesh->GetBoundingBox();
    const FVector Size = Box.GetSize();
    const double Tolerance = FMath::Max(0.01, Size.GetMax() * 0.001);
    for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
    {
        FVector Corner[3];
        FVector Normal = FVector::ZeroVector;
        for (int32 K = 0; K < 3; ++K)
        {
            const uint32 Vertex = Indices[Index + K];
            const FVector3f& Position = Positions.VertexPosition(Vertex);
            const FVector4f TangentZ = Tangents.VertexTangentZ(Vertex);
            Corner[K] = FVector(Position.X, Position.Y, Position.Z);
            Normal += FVector(TangentZ.X, TangentZ.Y, TangentZ.Z);
        }
        Normal = Normal.GetSafeNormal();
        const double Area = 0.5 * FVector::CrossProduct(Corner[1] - Corner[0], Corner[2] - Corner[0]).Size();
        for (int32 Side = 0; Side < 6; ++Side)
        {
            const int32 Axis = Side / 2;
            const bool bMax = (Side % 2) == 1;
            const double Plane = bMax ? Box.Max[Axis] : Box.Min[Axis];
            const bool bOnPlane = FMath::Abs(Corner[0][Axis] - Plane) < Tolerance &&
                                  FMath::Abs(Corner[1][Axis] - Plane) < Tolerance &&
                                  FMath::Abs(Corner[2][Axis] - Plane) < Tolerance;
            if (bOnPlane && (bMax ? Normal[Axis] : -Normal[Axis]) > 0.5)
            {
                Fill.Share[Side] += Area;
            }
        }
    }
    for (int32 Side = 0; Side < 6; ++Side)
    {
        const int32 Axis = Side / 2;
        const double SideArea = Size[(Axis + 1) % 3] * Size[(Axis + 2) % 3];
        Fill.Share[Side] = SideArea > 1e-4 ? Fill.Share[Side] / SideArea : 0.0;
    }
    return Fill;
}
} // namespace McpCoplanar
