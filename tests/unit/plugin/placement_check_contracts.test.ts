import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const placementCheck = (): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorPlacementCheck.cpp',
    ),
    'utf8',
  );

// Stage 2's shelf legs poke 12 units into the deck they hold up. The ground trace started inside
// the deck and took it for the floor, so each leg read as "sunk 620 units" with a suggestedZ on
// top of the deck: a fix that would have wrecked the level.
describe('audit_placement finds the floor under an actor, not what it holds up', () => {
  it('skips a hit whose bottom sits in the upper half of the actor', () => {
    expect(placementCheck()).toContain('if (HitBottomZ >= Origin.Z) {');
  });
});

// Stage 9's blimps "intersected" the mountain peak by 270 and 340 units: the cone's box is mostly
// air. The engine's shape overlap skips trimesh pieces, so a complex-as-simple mesh must keep the
// box verdict instead of always reading "apart".
describe('audit_placement confirms a box overlap on the real shapes', () => {
  it('drops a pair whose collision shapes do not touch', () => {
    const source = placementCheck();
    expect(source).toContain('if (McpShapesApart(Actor, Other)) {');
    expect(source).toContain('B->ComponentOverlapComponent(A, A->GetComponentLocation(), A->GetComponentQuat(), Params)');
  });

  it('only trusts shapes the engine test can move', () => {
    expect(placementCheck()).toContain('Setup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple');
  });
});

// "intersects 4 actor(s)" named only the deepest, and no read call describes one actor's placement.
describe('audit_placement rows list every actor a finding intersects', () => {
  it('carries overlappingActors from the placement check into the row', () => {
    const audit = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorPlacementAudit.cpp',
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
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorPlacementAudit.cpp',
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
  const root = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor';
  const read = (...segments: string[]): string => readFileSync(resolve(process.cwd(), root, ...segments), 'utf8');
  const flat = (source: string): string => source.split(/\s+/u).join(' ');

  it('an actor held from the side says what it is mounted on, and is neither floating nor without a surface below', () => {
    const source = flat(placementCheck());

    expect(source).toContain('#include "Domains/ControlActor/Placement/McpAutomationBridge_PlacementMount.h"');
    expect(source).toContain('} else if (Clearance > 50.0) {');
    expect(source).toContain('if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) { Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount)); } else { Notes.Add(FString::Printf( TEXT("floating %.0f units above the surface under it"), Clearance)); }');
    expect(source).toContain('} else if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) { Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount)); } else { Notes.Add(TEXT("nothing below it - it may be outside the playable area")); }');
    expect(source, 'the volume-only predicate moved beside the mount search').not.toContain('McpIsBoundsOnlyActor');
  });

  it('touching or overlapping a vertical face within the graze distance is mounted; a floor, a roof edge or the inside of a big box is not', () => {
    const source = flat(read('Placement', 'McpAutomationBridge_PlacementMount.cpp'));

    expect(source).toContain('if (!Other || Other == Actor || Other->IsHidden() || IsBoundsOnlyActor(Other)) { continue; }');
    expect(source).toContain('const FBox Reach = ActorBox.ExpandBy(Touch);');
    expect(source).toContain('const FBox Shared = Reach.Overlap(FBox::BuildAABB(Origin, Extent));');
    expect(source).toContain('if (Extent.GetMin() <= 1.0 || !Shared.IsValid) { continue; }');
    expect(source).toContain('const bool bAcrossX = Size.X <= Size.Y;');
    expect(source).toContain('if (Depth < Size.Z && Depth <= FMath::Max(2.0 * Touch, 0.25 * (bAcrossX ? Own.X : Own.Y))) { return Other; }');
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
    expect(read('McpAutomationBridge_ControlActorPlacementAudit.cpp')).toContain('if (!Entry->HasField(TEXT("mountedOn")) && Entry->TryGetNumberField(TEXT("groundClearance"), Clearance)) {');
  });
});
