// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class UObject;

// Whether a mesh's geometry is sound, read from its source triangles: inspect check_mesh reports it, and the import and
// reimport replies warn with it. Corners at one place (to 0.01 cm) are welded, and each part (faces joined by welded
// corners) is judged on its own: a closed part (no open edge, no edge of three faces) whose faces enclose a negative
// volume is wound inside out. Its faces point inward, so the renderer culls the side a viewer sees and draws the far
// inner wall; a tool that draws both sides (Blender) shows it whole. A separate inner shell of a hollow (a cavity wall)
// reads the same way. Linear in triangles: hash maps over corners and edges.
namespace McpMeshCheck
{
struct FPart
{
    int32 Triangles = 0;
    FBox Box = FBox(ForceInit);
    double Volume = 0.0; // signed, positive when wound outward; only a closed part encloses one
    bool bClosed = false;
    bool bInsideOut = false;
};

struct FReport
{
    int32 Triangles = 0, Vertices = 0, OpenEdges = 0, Holes = 0, SeamEdges = 0, NonManifoldEdges = 0;
    int32 FlippedEdges = 0, DegenerateTriangles = 0, InsideOutParts = 0;
    double SurfaceArea = 0.0;
    double Volume = 0.0; // the closed parts' volumes, each counted by its size
    FBox Bounds = FBox(ForceInit);
    TArray<FPart> Parts; // the inside-out parts first, then the rest, each group largest first
    bool IsClosed() const { return Triangles > 0 && OpenEdges == 0 && NonManifoldEdges == 0; }
};

/** Corners holds three indices into Positions per triangle, in the order Unreal draws a face's front. */
FReport Analyze(const TArray<FVector3f>& Positions, const TArray<int32>& Corners);

/** LOD 0's source triangles: a static mesh's mesh description, or a skeletal mesh's imported model in its reference
 *  pose with one corner per point of the source (render vertices split at UV seams are joined again). False for any
 *  other object, for one without source geometry, and for one of more than MaxTriangles. */
bool ReadSourceTriangles(const UObject* Mesh, TArray<FVector3f>& OutPositions, TArray<int32>& OutCorners,
                         int32 MaxTriangles = MAX_int32);

/** Appends to Result's warnings one line per static or skeletal mesh among Assets that has inside-out parts. */
void AppendInsideOutWarnings(const TArray<UObject*>& Assets, const TSharedPtr<FJsonObject>& Result);
} // namespace McpMeshCheck
