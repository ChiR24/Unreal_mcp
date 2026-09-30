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
