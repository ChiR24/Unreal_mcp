// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpPlacementShapes.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"

namespace McpPlacement {

// A trigger or pickup volume overlaps what enters it and blocks nothing, so it
// cannot interpenetrate anything: a pawn standing in a trigger box read as
// intersecting the trigger's actor.
bool McpBlocksSolids(const UPrimitiveComponent *Comp) {
  for (const ECollisionChannel Channel : {ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn}) {
    if (Comp->GetCollisionResponseToChannel(Channel) == ECR_Block) {
      return true;
    }
  }
  return false;
}

// Collision switched on is not a body. A water mesh, a text render or a mesh
// converted with no collision has nothing in the physics scene to strike or
// stand on, so it cannot interpenetrate anything: a pawn standing in a water
// zone read "intersects WaterZone by 68 units" on every move. An instanced mesh
// keeps a body per instance instead, so it still counts.
bool McpHasBody(const UPrimitiveComponent *Comp) {
  if (Comp->IsA<UInstancedStaticMeshComponent>()) {
    return static_cast<const UInstancedStaticMeshComponent *>(Comp)->GetInstanceCount() > 0;
  }
  const FBodyInstance *Body = Comp->GetBodyInstance();
  return Body && Body->IsValidBodyInstance();
}

namespace {

// Only a plain static mesh is checked shape to shape; instanced and skeletal
// bodies need other paths.
const UBodySetup *McpMeshBodySetup(const UPrimitiveComponent *Comp) {
  const UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Comp);
  if (!Mesh || Mesh->IsA<UInstancedStaticMeshComponent>() || !Mesh->GetStaticMesh() ||
      !Comp->GetBodyInstance() || !Comp->GetBodyInstance()->IsValidBodyInstance()) {
    return nullptr;
  }
  return Mesh->GetStaticMesh()->GetBodySetup();
}

// The engine queries with a body's convex shapes only, and a complex-as-simple
// mesh has none: its triangles stand in for them.
bool McpCanQuery(const UPrimitiveComponent *Comp) {
  const UBodySetup *Setup = McpMeshBodySetup(Comp);
  return Setup && Setup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple && Setup->AggGeom.GetElementCount() > 0;
}

// What a query can strike: convex shapes, or a complex-as-simple mesh's
// triangles, which the engine marks simple too. Store packs and baked meshes are
// mostly the latter, and asking for convex shapes on BOTH sides left every pair
// with one of them to the boxes: a lamp post between parked cars read as
// intersecting four of them, a queue barrier under an entrance canopy as
// intersecting the building.
bool McpCanBeStruck(const UPrimitiveComponent *Comp) {
  const UBodySetup *Setup = McpMeshBodySetup(Comp);
  return Setup && (Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple || Setup->AggGeom.GetElementCount() > 0);
}

// The mesh's own box, turned and scaled with it, holds every triangle: when that
// box strikes nothing of Struck, the mesh does not either. Two triangle meshes
// cannot be tested against each other any other way.
bool McpBoxApartFrom(const UPrimitiveComponent *BoxOf, const UPrimitiveComponent *Struck) {
  const UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(BoxOf);
  if (!Mesh || !Mesh->GetStaticMesh() || !McpCanBeStruck(Struck)) {
    return false;
  }
  const FBoxSphereBounds Local = Mesh->GetStaticMesh()->GetBounds();
  const FTransform &Xf = BoxOf->GetComponentTransform();
  const FCollisionShape Box = FCollisionShape::MakeBox(Local.BoxExtent * Xf.GetScale3D().GetAbs());
  return !Struck->GetBodyInstance()->OverlapTest(Xf.TransformPosition(Local.Origin), Xf.GetRotation(), Box);
}

// ponytail: a triangle mesh is a surface, so a part wholly inside its solid,
// touching no triangle, reads apart; add an inside test if buried parts matter.
bool McpPairApart(UPrimitiveComponent *A, UPrimitiveComponent *B, const FCollisionQueryParams &Params) {
  if (McpCanQuery(A) && McpCanBeStruck(B)) {
    return !B->ComponentOverlapComponent(A, A->GetComponentLocation(), A->GetComponentQuat(), Params);
  }
  if (McpCanQuery(B) && McpCanBeStruck(A)) {
    return !A->ComponentOverlapComponent(B, B->GetComponentLocation(), B->GetComponentQuat(), Params);
  }
  return McpBoxApartFrom(A, B) || McpBoxApartFrom(B, A);
}

} // namespace

// Boxes only say where an actor could be. A cone's box is mostly air, so a blimp
// flying past a mountain peak read "intersects by 340 units". Apart only when
// every colliding component pair was checked on its real shapes and none touch;
// a pair that cannot be checked keeps the box verdict.
bool McpShapesApart(AActor *Actor, AActor *Other) {
  TInlineComponentArray<UPrimitiveComponent *> Mine(Actor), Theirs(Other);
  const FCollisionQueryParams Params(SCENE_QUERY_STAT(McpPlacementShapes), false);
  for (UPrimitiveComponent *A : Mine) {
    for (UPrimitiveComponent *B : Theirs) {
      if (!A->IsCollisionEnabled() || !B->IsCollisionEnabled() || !McpBlocksSolids(A) || !McpBlocksSolids(B) ||
          !McpHasBody(A) || !McpHasBody(B) || !A->Bounds.GetBox().Intersect(B->Bounds.GetBox())) {
        continue;
      }
      if (!McpPairApart(A, B, Params)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace McpPlacement
