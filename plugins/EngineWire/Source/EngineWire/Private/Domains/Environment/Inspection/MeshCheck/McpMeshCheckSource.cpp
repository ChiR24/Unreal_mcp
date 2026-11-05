// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Inspection/MeshCheck/McpMeshCheck.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "UObject/Package.h"

namespace McpMeshCheck
{
namespace
{
// An import or reimport checks meshes up to this many triangles before it answers (well under a second of hashing);
// a larger one is left to check_mesh, which reads any size.
constexpr int32 ImportCheckMaxTriangles = 1000000;

bool ReadStaticTriangles(const UStaticMesh& Mesh, TArray<FVector3f>& OutPositions, TArray<int32>& OutCorners, int32 MaxTriangles)
{
    const FMeshDescription* Description = Mesh.GetMeshDescription(0);
    if (!Description || Description->Triangles().Num() > MaxTriangles)
    {
        return false;
    }
    const TVertexAttributesRef<const FVector3f> Positions = Description->GetVertexPositions();
    OutPositions.SetNumZeroed(Description->Vertices().GetArraySize());
    for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
    {
        OutPositions[Vertex.GetValue()] = Positions[Vertex];
    }
    OutCorners.Reserve(Description->Triangles().Num() * 3);
    for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
    {
        for (const FVertexID Corner : Description->GetTriangleVertices(Triangle))
        {
            OutCorners.Add(Corner.GetValue());
        }
    }
    return true;
}

// LOD 0 of the imported model, the same on every 5.x editor: its sections' soft vertices hold the reference pose, and
// its index buffer covers every section with indices into them in section order. A render vertex is split wherever a
// UV, normal or weight changes; the source point it came from (MeshToImportVertexMap) is what its faces share, as a
// static mesh's vertex is. Without that map, the render vertices themselves.
bool ReadSkeletalTriangles(const USkeletalMesh& Mesh, TArray<FVector3f>& OutPositions, TArray<int32>& OutCorners, int32 MaxTriangles)
{
    const FSkeletalMeshModel* Model = Mesh.GetImportedModel();
    if (!Model || !Model->LODModels.IsValidIndex(0) || Model->LODModels[0].IndexBuffer.Num() / 3 > MaxTriangles)
    {
        return false;
    }
    const FSkeletalMeshLODModel& Lod = Model->LODModels[0];
    const TArray<int32>& Map = Lod.MeshToImportVertexMap;
    const bool bByPoint = Map.Num() == static_cast<int32>(Lod.NumVertices);
    const auto Point = [&Map, bByPoint](int32 Vertex) { return !bByPoint ? Vertex : Map.IsValidIndex(Vertex) ? Map[Vertex] : INDEX_NONE; };
    int32 SoftVertices = 0;
    for (const FSkelMeshSection& Section : Lod.Sections)
    {
        SoftVertices += Section.SoftVertices.Num();
        for (int32 Index = 0; Index < Section.SoftVertices.Num(); ++Index)
        {
            const int32 Source = Point(static_cast<int32>(Section.BaseVertexIndex) + Index);
            if (Source < 0)
            {
                return false;
            }
            if (Source >= OutPositions.Num())
            {
                OutPositions.SetNumZeroed(Source + 1);
            }
            OutPositions[Source] = Section.SoftVertices[Index].Position;
        }
    }
    if (SoftVertices != static_cast<int32>(Lod.NumVertices))
    {
        return false;
    }
    OutCorners.Reserve(Lod.IndexBuffer.Num());
    for (const uint32 Vertex : Lod.IndexBuffer)
    {
        const int32 Source = Point(static_cast<int32>(Vertex));
        if (!OutPositions.IsValidIndex(Source))
        {
            return false;
        }
        OutCorners.Add(Source);
    }
    return true;
}
} // namespace

bool ReadSourceTriangles(const UObject* Mesh, TArray<FVector3f>& OutPositions, TArray<int32>& OutCorners, int32 MaxTriangles)
{
    OutPositions.Reset();
    OutCorners.Reset();
    if (const UStaticMesh* Static = Cast<UStaticMesh>(Mesh))
    {
        return ReadStaticTriangles(*Static, OutPositions, OutCorners, MaxTriangles);
    }
    const USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Mesh);
    return Skeletal && ReadSkeletalTriangles(*Skeletal, OutPositions, OutCorners, MaxTriangles);
}

// A character exported from a tool that draws both sides of every face (Blender) looked right there with its head
// wound inside out, and showed the inside of its head in Unreal: nothing in the import reply said so.
void AppendInsideOutWarnings(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    const int32 Before = Warnings.Num();
    TArray<FVector3f> Positions;
    TArray<int32> Corners;
    for (const UObject* Asset : Assets)
    {
        if (!ReadSourceTriangles(Asset, Positions, Corners, ImportCheckMaxTriangles))
        {
            continue;
        }
        const int32 InsideOut = Analyze(Positions, Corners).InsideOutParts;
        if (InsideOut > 0)
        {
            const FString Path = Asset->GetPackage()->GetName();
            Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s has %d inside-out part(s): their faces point "
                "inward, so the renderer culls the side you look at and shows the far inner wall. Flip their normals in the "
                "source and import again; inspect.check_mesh with meshPath %s lists them."), *Path, InsideOut, *Path)));
        }
    }
    if (Warnings.Num() > Before)
    {
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
}
} // namespace McpMeshCheck
