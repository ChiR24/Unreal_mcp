import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const placementCheck = (): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor/McpActorPlacementCheck.cpp',
    ),
    'utf8',
  );

// Stage 2's shelf legs poke 12 units into the deck they hold up. The ground trace started inside
// the deck and took it for the floor, so each leg read as "sunk 620 units" with a suggestedZ on
// top of the deck: a fix that would have wrecked the level.
describe('audit_placement finds the floor under an actor, not what it holds up', () => {
  it('skips a hit whose bottom sits in the upper half of the actor', () => {
    expect(placementCheck()).toContain('if (HitBottomZ >= Centre.Z) {');
  });

  // A crate stacked on a simulated crate read "floating" above the floor under both: a physics body is a PhysicsBody.
  it('counts a simulated body as a surface', () => {
    expect(placementCheck()).toContain('Solids.AddObjectTypesToQuery(ECC_PhysicsBody);');
  });

  // A BlockingVolume standing in a ground slab: the trace began inside it and
  // took its own start for the floor, so the slab read "sunk 160 units".
  it('never takes an invisible volume for the floor', () => {
    const source = placementCheck();
    expect(source).toContain('#include "GameFramework/Volume.h"');
    expect(source).toContain('if (!Surface || !HitActor || HitActor->IsA<AVolume>() ||');
  });
});

// Stage 9's blimps "intersected" the mountain peak by 270 and 340 units: the cone's box is mostly
// air. The engine queries with convex shapes only, but it can strike a complex-as-simple mesh's
// triangles; asking for convex shapes on both sides left a lamp post between parked cars
// "intersecting" four of them and a barrier under an entrance canopy "intersecting" the building.
describe('audit_placement confirms a box overlap on the real shapes', () => {
  const shapes = (): string =>
    readFileSync(
      resolve(
        process.cwd(),
        'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor/Placement/McpPlacementShapes.cpp',
      ),
      'utf8',
    );

  it('drops a pair whose collision shapes do not touch', () => {
    const source = placementCheck();
    expect(source).toContain('#include "Domains/ControlActor/Placement/McpPlacementShapes.h"');
    expect(source).toContain('if (McpShapesApart(Actor, Other)) {');
    expect(shapes()).toContain('return !B->ComponentOverlapComponent(A, A->GetComponentLocation(), A->GetComponentQuat(), Params);');
  });

  it('queries with convex shapes and strikes convex shapes or complex-as-simple triangles, either way round', () => {
    const source = shapes();
    expect(source).toContain('return Setup && Setup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple && Setup->AggGeom.GetElementCount() > 0;');
    expect(source).toContain('return Setup && (Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple || Setup->AggGeom.GetElementCount() > 0);');
    expect(source).toContain('return !A->ComponentOverlapComponent(B, B->GetComponentLocation(), B->GetComponentQuat(), Params);');
  });

  it('tests two triangle meshes by one mesh\'s turned box against the other', () => {
    const source = shapes();
    expect(source).toContain('return McpBoxApartFrom(A, B) || McpBoxApartFrom(B, A);');
    expect(source).toContain('return !Struck->GetBodyInstance()->OverlapTest(Center, Rotation, Box);');
    // On the mesh bounds' origin, turned with the mesh (a corner-pivoted kit piece keeps it off its pivot): the one box
    // fix_placement measures its pushes with.
    expect(source).toContain('OutCenter = Xf.TransformPosition(Local.Origin);');
    expect(source).toMatch(/bool McpQueryShape\([\s\S]*?McpMeshBox\(Mesh, OutShape, OutCenter, OutRotation\);/u);
  });
});

// Seven buoys floating in an ocean each read "intersects the ocean by 47; sunk 47 units below the surface; z=-3 would
// rest it": following the suggestion would sit a floating body on top of the water like a deck.
describe('the placement check treats a water body as a surface, not a solid', () => {
  it('never reports an overlap with a water body, nor checks one as the subject', () => {
    const source = placementCheck();
    expect(source).toContain('if (Class->GetFName() == TEXT("WaterBody")) {');
    expect(source).toContain('IsBoundsOnlyActor(Other) || McpPlacementAccepted(Other) || McpIsWaterBody(Other)) {');
    expect(source).toContain('McpIsOverlapOnly(Actor) || McpIsWaterBody(Actor)) {');
  });

  it('an actor below a water surface is not sunk and gets no suggested location', () => {
    const source = placementCheck();
    expect(source).toContain('bOutOnWater = McpIsWaterBody(HitActor);');
    expect(source).toContain('const bool bDeep = Gap < -Sample.SunkFloor && !bOnWater;');
  });
});

// create_road_spline laid plan_road's 36 points over a hill and its reply said "'Road' nothing below it" plus a -X face
// "overlapping 100 x 100" with itself; one call later the same road read clean, raised 3 m or lowered 1 m too.
describe('a spline mesh actor is judged where its segments lie, on the bounds the call left', () => {
  const placementRoot = 'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor/Placement';
  const readPlacement = (name: string): string => readFileSync(resolve(process.cwd(), placementRoot, name), 'utf8');
  const flat = (source: string): string => source.split(/\s+/u).join(' ');

  // Bounds follow a reshaped part only when its render state is rebuilt at the end of the frame, after the reply.
  it('recomputes the bounds of what it judges before reading them', () => {
    const source = placementCheck();
    expect(source).toContain('Part->UpdateBounds();');
    const refresh = source.indexOf('McpRefreshBounds(Actor);');
    expect(refresh).toBeGreaterThan(-1);
    expect(refresh).toBeLessThan(source.indexOf('Actor->GetActorBounds(true, Origin, Extent);'));
    const watch = readPlacement('McpChangeWatch.cpp');
    const watchRefresh = watch.indexOf('McpPlacement::McpRefreshBounds(Actor);');
    expect(watchRefresh, 'every touched actor, before any is fingerprinted or judged').toBeGreaterThan(-1);
    expect(watchRefresh).toBeLessThan(watch.indexOf('const auto Consider ='));
  });

  it('traces the ground under every section of a spline mesh and under one box round the other parts', () => {
    const ground = flat(readPlacement('McpPlacementGround.cpp'));
    expect(ground).toContain('if (const USplineMeshComponent *Spline = Cast<USplineMeshComponent>(Part)) {');
    expect(ground).toContain('Spline.CalcSliceTransform(static_cast<float>(End ? Local.Max[Forward] : Local.Min[Forward]))');
    // Halfway on the straight line between the end sections: a stock cube segment is drawn and collides that way.
    expect(ground).toContain('Middle[Corner] = (Ends[0][Corner] + Ends[1][Corner]) * 0.5;');
    expect(ground, 'an actor with no spline mesh keeps its one box').toContain('if (!bAnySpline) { Rest = SelfBox; }');
    // A section is a slab when far wider than thick: a road may sink its own thickness before the terrain covers it.
    expect(ground).toContain('Sample.SunkFloor = Thick * 4.0 < Wide ? FMath::Max(Sample.Graze, Thick) : Sample.Graze;');
    const source = flat(placementCheck());
    expect(source).toContain('McpGroundSamples(*Actor, SelfBox, Samples);');
    expect(source).toContain('McpGroundUnder(World, Actor, SelfBox.Max.Z, Sample, SampleGroundZ, bOnWater)');
    // Each section is traced from the road's top: a canopy or a roof over a low section is not the ground under it.
    expect(source).toContain('if (Candidate.ImpactPoint.Z > Sample.Box.Max.Z && !McpIsGroundLike(HitActor)) {');
    expect(source).toContain('if (!bHasGround || (bDeep && !bSunk) || (bDeep == bSunk && Gap < Clearance)) {');
  });

  // The verdict on plain numbers (a model of the loop pinned above): each sample is its underside's height over the
  // ground and the depth it may be bedded. The planned road's sections sat 6 over the ground at its points, but the
  // terrain rose over its top at 13 midpoints (underside 19.9 below the ground at the worst).
  type Sample = { gap: number; floor: number; water?: boolean };
  const verdict = (samples: readonly Sample[]): string => {
    let best: { gap: number; deep: boolean } | undefined;
    for (const { gap, floor, water } of samples) {
      const deep = gap < -floor && !water;
      if (!best || (deep && !best.deep) || (deep === best.deep && gap < best.gap)) best = { gap, deep };
    }
    if (!best) return 'nothing below';
    return best.deep ? `sunk ${Math.round(-best.gap)}` : best.gap > 50 ? `floating ${Math.round(best.gap)}` : 'clean';
  };
  const road = (lift: number): Sample[] => [6, 6, -7.3, 6, -19.9, 6, -14.6].map((gap) => ({ gap: gap + lift, floor: 8 }));

  it('names the deepest sunk section, else floats only when no section touches the ground', () => {
    expect(verdict(road(0)), 'terrain over the road top at its midpoints').toBe('sunk 20');
    expect(verdict(road(20)), 'the same road lifted 20, resting on the terrain everywhere').toBe('clean');
    expect(verdict(road(300))).toBe('floating 280');
    expect(verdict(road(-100))).toBe('sunk 120');
    expect(verdict([{ gap: -30, floor: 8, water: true }, { gap: 6, floor: 8 }]), 'under a water surface is afloat').toBe('clean');
    expect(verdict([])).toBe('nothing below');
  });

  // A landscape's box runs from its lowest valley to its highest peak: a cube resting on its flat part read
  // "intersects 'RoadProbeLand' by 100", and the landscape "intersected" a ball of 10 m standing on it.
  it('never measures an overlap against a landscape box, either way round', () => {
    const source = placementCheck();
    expect(source).toContain('#include "LandscapeProxy.h"');
    expect(source).toContain('if (Other->IsA<ALandscapeProxy>() || Actor->IsA<ALandscapeProxy>()) {');
  });

  // Every segment of a road read as the same stock cube at the actor's origin: the faces of a mesh bent along a curve are
  // not its mesh's box under its transform, and neither is the box fix_placement and the overlap test measure with.
  it('never models a spline mesh as its mesh box under its transform', () => {
    expect(flat(readPlacement('McpCoplanarFaces.cpp'))).toContain(
      'Component->IsA<UInstancedStaticMeshComponent>() || Component->IsA<USplineMeshComponent>() ||');
    const shapes = readPlacement('McpPlacementShapes.cpp');
    expect(shapes).toContain('if (!Mesh || !Mesh->GetStaticMesh() || Mesh->IsA<USplineMeshComponent>() || !McpCanBeStruck(Struck)) {');
    expect(shapes).toContain('if (!Mesh || Mesh->IsA<UInstancedStaticMeshComponent>() || Mesh->IsA<USplineMeshComponent>() || !Mesh->GetStaticMesh()) {');
  });
});

// "intersects 4 actor(s)" named only the deepest, and no read call describes one actor's placement.
describe('audit_placement rows list every actor a finding intersects', () => {
  it('carries overlappingActors from the placement check into the row', () => {
    const audit = readFileSync(
      resolve(
        process.cwd(),
        'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor/McpActorPlacementAudit.cpp',
      ),
      'utf8',
    );
    expect(audit).toContain('Object->SetArrayField(TEXT("overlappingActors"), Finding.Overlaps);');
  });

  // Actors sharing a label are reported by object name, so nameFilter must match that name too.
  it('filters by the object name it reports as well as the label', () => {
    const audit = readFileSync(
      resolve(
        process.cwd(),
        'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor/McpActorPlacementAudit.cpp',
      ),
      'utf8',
    );
    expect(audit).toContain('!Actor->GetName().Contains(NameFilter)');
  });
});

// Only support from below was looked for, so everything mounted on a wall read "floating" by the height of the wall it hangs on:
// 20 window bands flush on a hall (1 unit into its face) and 96 awnings on a facade, "floating 139 / 521 units". The reply
// came from set_transform, and spawn and audit_placement share the check.
describe('the placement check does not call a wall-mounted actor floating', () => {
  const root = 'plugins/EngineWire/Source/EngineWire/Private/Domains/ControlActor';
  const read = (...segments: string[]): string => readFileSync(resolve(process.cwd(), root, ...segments), 'utf8');
  const flat = (source: string): string => source.split(/\s+/u).join(' ');

  it('an actor held from the side says what it is mounted on, and is neither floating nor without a surface below', () => {
    const source = flat(placementCheck());

    expect(source).toContain('#include "Domains/ControlActor/Placement/McpPlacementMount.h"');
    expect(source).toContain('} else if (Clearance > 50.0) {');
    expect(source).toContain('if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) { Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount)); } else { Notes.Add(FString::Printf( TEXT("floating %.0f units above the surface under it"), Clearance)); }');
    // A floor or a platform over a pit (slab-shaped) is ground itself: the first floor of every new level read
    // "nothing below it - it may be outside the playable area".
    expect(source).toContain('} else if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) { Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount)); } else if (!McpIsGroundLike(Actor)) {');
    expect(source).toContain('Notes.Add(TEXT("nothing below it - it may be outside the playable area")); }');
    expect(source, 'the volume-only predicate moved beside the mount search').not.toContain('McpIsBoundsOnlyActor');
  });

  it('touching or overlapping a vertical face within the graze distance is mounted; a floor, a roof edge or the inside of a big box is not', () => {
    const source = flat(read('Placement', 'McpPlacementMount.cpp'));

    expect(source).toContain('if (!Other || Other == Actor || Other->IsHidden() || IsBoundsOnlyActor(Other)) { continue; }');
    expect(source).toContain('if (HangsOn(ActorBox, FBox::BuildAABB(Origin, Extent), Touch)) { return Other; }');
    // The pair test, shared with describe_relations' mounted.
    expect(source).toContain('const FBox Shared = ActorBox.ExpandBy(Touch).Overlap(OtherBox);');
    expect(source).toContain('if (OtherBox.GetExtent().GetMin() <= 1.0 || !Shared.IsValid) { return false; }');
    expect(source).toContain('const bool bAcrossX = Size.X <= Size.Y;');
    expect(source).toContain('return Depth < Size.Z && Depth <= FMath::Max(2.0 * Touch, 0.25 * (bAcrossX ? Own.X : Own.Y));');
  });

  // The rule above on plain boxes, with the numbers of the live cases and the ones it must keep reporting.
  // (A model of FindMount: the C++ is pinned by the assertions above.)
  type Box = { min: readonly number[]; max: readonly number[] };
  const box = (x: readonly [number, number], y: readonly [number, number], z: readonly [number, number]): Box => ({ min: [x[0], y[0], z[0]], max: [x[1], y[1], z[1]] });
  const sizeOf = (b: Box): number[] => b.max.map((v, i) => v - (b.min[i] ?? 0));
  const mounted = (self: Box, host: Box): boolean => {
    const own = sizeOf(self);
    const touch = Math.max(4, Math.min(...own) / 2 * 0.12);
    const reach = { min: self.min.map((v) => v - touch), max: self.max.map((v) => v + touch) };
    const min = reach.min.map((v, i) => Math.max(v, host.min[i] ?? 0));
    const max = reach.max.map((v, i) => Math.min(v, host.max[i] ?? 0));
    if (min.some((v, i) => v > (max[i] ?? 0))) return false;
    const [sx = 0, sy = 0, sz = 0] = max.map((v, i) => v - (min[i] ?? 0));
    const acrossX = sx <= sy;
    const depth = acrossX ? sx : sy;
    return depth < sz && depth <= Math.max(2 * touch, 0.25 * (acrossX ? own[0] ?? 0 : own[1] ?? 0));
  };
  const hall = box([-1000, 0], [-1000, 1000], [0, 1500]);

  it('a window band 1 unit into a hall face and an awning touching a facade are mounted', () => {
    expect(mounted(box([-1, 4], [-200, 200], [300, 320]), hall)).toBe(true);
    expect(mounted(box([0, 150], [-200, 200], [500, 510]), hall)).toBe(true);
    expect(mounted(box([2, 7], [-200, 200], [300, 320]), hall), 'a gap smaller than the graze distance').toBe(true);
  });

  it('a platform away from a wall, a prop on a table top and a pole inside a big box are not', () => {
    expect(mounted(box([100, 500], [0, 400], [200, 220]), hall), 'a platform 100 units off the wall').toBe(false);
    expect(mounted(box([50, 80], [20, 50], [80, 110]), box([0, 200], [0, 100], [0, 80])), 'resting on a table: the contact is a floor').toBe(false);
    expect(mounted(box([-10, 10], [-10, 10], [1000, 1300]), box([-5000, 5000], [-5000, 5000], [0, 3000])), 'floating inside a landscape box').toBe(false);
  });

  it('audit_placement does not count the floor under a mounted actor as its error', () => {
    expect(read('McpActorPlacementAudit.cpp')).toContain('if (!Entry->HasField(TEXT("mountedOn")) && Entry->TryGetNumberField(TEXT("groundClearance"), Clearance)) {');
  });
});
