// Plan Todo 13 / T13-G3 (BB-019, BB-060) - the shared geometry alias mapping.
//
// Added after an independent verifier proved the mapping was unguarded: deleting
// `copyAlias(normalized, args, 'targetActor', 'actorName')` from
// geometry-handlers.ts left every other suite green, so a refactor could have
// silently re-broken all 28 geometry actions with CI still passing.
//
// Native reads `actorName` and never reads `targetActor` as a payload field, so
// the dispatched payload - not the record - is what has to carry actorName.
//
// The bridge dispatch is mocked in THIS file only; the sibling
// todo13-canonical-contracts.test.ts deliberately uses a throwing bridge to
// prove its gates refuse pre-dispatch, and mocking there would defeat that.

import { describe, expect, it, vi } from 'vitest';

import type { ITools } from '../../../src/types/tools/tool-interfaces.js';

const hoisted = vi.hoisted(() => ({
  dispatched: [] as Array<{ tool: string; args: Record<string, unknown> }>
}));

vi.mock('../../../src/tools/handlers/foundation/dispatch/common-handlers.js', async (importOriginal) => {
  const actual = await importOriginal<Record<string, unknown>>();
  return {
    ...actual,
    executeAutomationRequest: async (
      _tools: unknown,
      tool: string,
      args: Record<string, unknown>
    ): Promise<Record<string, unknown>> => {
      hoisted.dispatched.push({ tool, args });
      return { success: true };
    }
  };
});

const { handleGeometryTools } = await import('../../../src/tools/handlers/geometry/geometry-handlers.js');

const tools = {} as ITools;

async function dispatchedArgs(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  hoisted.dispatched.length = 0;
  const action = String(args.action);
  await handleGeometryTools(action, args, tools);
  expect(hoisted.dispatched).toHaveLength(1);
  expect(hoisted.dispatched[0]?.tool).toBe('manage_geometry');
  return hoisted.dispatched[0]?.args ?? {};
}

describe('todo13 T13-G3: targetActor reaches Unreal as the actorName native reads', () => {
  it.each([
    'array_radial',
    'recalculate_normals',
    'array_linear',
    'simplify_mesh',
    'flip_normals'
  ])('%s: the declared targetActor alias is mapped onto actorName', async (action) => {
    const sent = await dispatchedArgs({ action, targetActor: 'DM_A', count: 4 });

    expect(sent.actorName).toBe('DM_A');
  });

  it('an explicit actorName wins over a conflicting targetActor (non-clobbering)', async () => {
    const sent = await dispatchedArgs({
      action: 'array_radial',
      actorName: 'DM_Explicit',
      targetActor: 'DM_Alias',
      count: 4
    });

    expect(sent.actorName).toBe('DM_Explicit');
  });

  it('leaves actorName absent when neither spelling is supplied', async () => {
    const sent = await dispatchedArgs({ action: 'flip_normals' });

    expect(sent.actorName).toBeUndefined();
  });

  it('still applies the per-action aliases alongside the shared mapping', async () => {
    const sent = await dispatchedArgs({
      action: 'recalculate_normals',
      targetActor: 'DM_A',
      hardEdgeAngle: 60,
      computeWeightedNormals: true
    });

    expect(sent.actorName).toBe('DM_A');
    expect(sent.splitAngle).toBe(60);
    expect(sent.areaWeighted).toBe(true);
  });
});
