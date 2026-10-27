// Copyright (c) 2024 EngineWire Contributors
//
// Where the placement check looks for the ground under an actor. One box round its colliding parts says where a compact
// actor stands, but a spline mesh is its mesh bent along a curve: a road of 35 segments up a hill was judged by its box's
// bottom (the low end of the road) against the ground under the box's middle (up the slope), so a road raised 3 m or
// lowered 1 m read as clean as one resting on the terrain. Each spline mesh is judged by its own cross-sections instead.

#include "Domains/ControlActor/Placement/McpPlacementMount.h"

#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace McpPlacement {
namespace {

// A cross-section of a spline mesh from its four corners in the world. Its thickness and width bound how deep it may be
// bedded, as a box's do: a road slab 8 thick and 600 wide may sink 8 into the terrain before the terrain covers it.
FMcpGroundSample McpSliceSample(const FVector (&Corners)[4]) {
  FMcpGroundSample Sample;
  Sample.Box = FBox(Corners, 4);
  const double Thick = Sample.Box.Max.Z - Sample.Box.Min.Z;
  const double Wide = FMath::Max((Corners[1] - Corners[0]).Size2D(), (Corners[2] - Corners[0]).Size2D());
  Sample.Graze = FMath::Max(4.0, FMath::Min(Thick, Wide) * 0.06);
  Sample.SunkFloor = Thick * 4.0 < Wide ? FMath::Max(Sample.Graze, Thick) : Sample.Graze;
  return Sample;
}

// The sections at both ends of the mesh (where CalcSliceTransform puts its first and last vertices) and halfway between
// them. Halfway is the straight line between the ends, not the curve: a stock cube has vertices at its ends only, so a
// segment is drawn, and collides, as the straight slab between its end sections.
void McpAddSplineSamples(const USplineMeshComponent &Spline, TArray<FMcpGroundSample> &Out) {
  const UStaticMesh *Mesh = Spline.GetStaticMesh();
  if (!Mesh) {
    return;
  }
  const FBox Local = Mesh->GetBoundingBox();
  const int32 Forward = static_cast<int32>(Spline.GetForwardAxis());
  const int32 Across = (Forward + 1) % 3;
  const int32 Up = (Forward + 2) % 3;
  const FTransform &ToWorld = Spline.GetComponentTransform();
  FVector Ends[2][4];
  for (int32 End = 0; End < 2; ++End) {
    const FTransform Slice = Spline.CalcSliceTransform(static_cast<float>(End ? Local.Max[Forward] : Local.Min[Forward]));
    for (int32 Corner = 0; Corner < 4; ++Corner) {
      // The forward coordinate stays 0: the slice puts the section on the spline.
      FVector Point = FVector::ZeroVector;
      Point[Across] = (Corner & 1) ? Local.Max[Across] : Local.Min[Across];
      Point[Up] = (Corner & 2) ? Local.Max[Up] : Local.Min[Up];
      Ends[End][Corner] = ToWorld.TransformPosition(Slice.TransformPosition(Point));
    }
    Out.Add(McpSliceSample(Ends[End]));
  }
  // ponytail: exact for the stock templates; a finely subdivided mesh on a tight curve is drawn along the curve, so its
  // ground is sampled up to the curve's bulge off the road. Sample CalcSliceTransform halfway too if that matters.
  FVector Middle[4];
  for (int32 Corner = 0; Corner < 4; ++Corner) {
    Middle[Corner] = (Ends[0][Corner] + Ends[1][Corner]) * 0.5;
  }
  Out.Add(McpSliceSample(Middle));
}

} // namespace

// A 24-thick floor disc with a decorative inlay bedded 8 units into it is correct construction, not a mistake. Without
// this distinction the hub reported 407 of 776 actors as broken, which is the same as reporting nothing.
bool McpIsSlab(const FVector &Extent) {
  return Extent.Z * 4.0 < FMath::Min(Extent.X, Extent.Y);
}

// Ignore a graze: meshes are authored to touch, and a shared edge is not a bug. The floor scales with the part so a large
// building is not flagged for the same absolute overlap that matters on a character. A slab set into the tier below it
// is inlay work; only a slab swallowed deeper than its own thickness is actually lost in the geometry.
FMcpGroundSample McpBoxSample(const FBox &Box) {
  const FVector Extent = Box.GetExtent();
  FMcpGroundSample Sample;
  Sample.Box = Box;
  Sample.Graze = FMath::Max(4.0, FMath::Min3(Extent.X, Extent.Y, Extent.Z) * 0.12);
  Sample.SunkFloor = McpIsSlab(Extent) ? FMath::Max(Sample.Graze, 2.0 * Extent.Z) : Sample.Graze;
  return Sample;
}

void McpGroundSamples(const AActor &Actor, const FBox &SelfBox, TArray<FMcpGroundSample> &OutSamples) {
  FBox Rest(ForceInit);
  bool bAnySpline = false;
  TInlineComponentArray<UPrimitiveComponent *> Parts(&Actor);
  for (const UPrimitiveComponent *Part : Parts) {
    if (!Part->IsRegistered() || !Part->IsCollisionEnabled()) {
      continue;
    }
    if (const USplineMeshComponent *Spline = Cast<USplineMeshComponent>(Part)) {
      McpAddSplineSamples(*Spline, OutSamples);
      bAnySpline = true;
    } else {
      Rest += Part->Bounds.GetBox();
    }
  }
  // Without a spline mesh the box is the one the overlap check measured, a player start's capsule included.
  if (!bAnySpline) {
    Rest = SelfBox;
  }
  if (Rest.IsValid) {
    OutSamples.Add(McpBoxSample(Rest));
  }
}

} // namespace McpPlacement
