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
});
