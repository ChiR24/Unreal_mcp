// tests/unit/capability-verdicts/capability-verdicts.test.ts
// Task 59 — the reading half of the UE 5.7.4 capability census, tested against
// payloads a real gateway actually produced.
//
// Every fixture below is a verbatim shape captured from the running server, not
// one invented to match the code. That distinction is the reason this file
// exists: the first version of the probe was written against an ASSUMED envelope,
// ran green against a live 5.7.4 editor, and produced a census claiming all 23
// canonical tools had lost all 1,335 of their actions. The run was green because
// nothing in a live certification can tell "the surface answered nothing" from
// "the reader looked in the wrong field". These assertions can.

import { describe, expect, it } from 'vitest';

import {
  CALLER_ERROR_CODES,
  atLeast,
  censusTool,
  classify,
  harvestActions,
  payloadOf,
  reachedEditor
} from './capability-verdicts.mjs';

/**
 * A real refusal from the live 5.7.4 plugin. The receipt fields are what matter:
 * `liveRevisions` is stamped by the editor on the game thread, so nothing
 * upstream of it can produce this envelope.
 */
const PLUGIN_REFUSAL = {
  result: {
    isError: true,
    content: [{ type: 'text', text: 'Error [INVALID_ARGUMENT]: actorName required' }],
    structuredContent: {
      capabilityId: 'manage_geometry.convert_to_nanite',
      capabilityRevision: 'bcd979e01f0a1b3c',
      correlationId: '16293FCF-AC53-4141-AAC8-13629ABA8168',
      status: 'error',
      liveRevisions: { selection: 1, level: 1, assetRegistry: 8148, package: 1 },
      success: false,
      operation: 'execute',
      message: 'actorName required',
      error: 'actorName required'
    }
  }
};

/** The same code, but refused by the gateway before anything was dispatched. */
const GATEWAY_REFUSAL = {
  result: {
    isError: true,
    structuredContent: {
      success: false,
      operation: 'execute',
      errorCode: 'INVALID_PARAMS',
      error: 'params must not override action or subAction.'
    }
  }
};

/**
 * A real `describe {tool}` result. The `content` block is the human rendering —
 * note that it ELIDES the action list — while `structuredContent` carries it in
 * full. Reading the wrong one is the whole defect this fixture pins.
 */
const DESCRIBE_TOOL_RESULT = {
  result: {
    content: [{
      type: 'text',
      text: 'success: true | operation: describe | tool: manage_geometry | actions: '
        + '[create_box, create_sphere, create_cylinder, ... (+73 more)] (76) | actionCount: 76'
    }],
    structuredContent: {
      success: true,
      operation: 'describe',
      tool: 'manage_geometry',
      actions: ['create_box', 'create_sphere', 'create_cylinder'],
      actionCount: 76,
      actionHasMore: false,
      perActionSchemas: false
    }
  }
};

describe('payloadOf', () => {
  it('prefers structuredContent over the elided human rendering', () => {
    const payload = payloadOf(DESCRIBE_TOOL_RESULT) as Record<string, unknown>;
    expect(payload.actions).toEqual(['create_box', 'create_sphere', 'create_cylinder']);
    expect(payload.actionCount).toBe(76);
  });

  it('does NOT fall back to the text block when structuredContent is present', () => {
    // The regression guard. If this ever returns { raw: ... } again, the census
    // silently reports every tool as having zero actions.
    const payload = payloadOf(DESCRIBE_TOOL_RESULT) as Record<string, unknown>;
    expect(payload.raw).toBeUndefined();
  });

  it('parses a JSON text block when there is no structuredContent', () => {
    const payload = payloadOf({
      result: { content: [{ type: 'text', text: '{"actions":["a"]}' }] }
    }) as Record<string, unknown>;
    expect(payload.actions).toEqual(['a']);
  });

  it('returns null rather than guessing when there is no payload at all', () => {
    expect(payloadOf({})).toBeNull();
    expect(payloadOf({ result: { content: [] } })).toBeNull();
  });
});

describe('harvestActions', () => {
  it('harvests a plain string array of actions', () => {
    expect([...harvestActions(payloadOf(DESCRIBE_TOOL_RESULT))].sort())
      .toEqual(['create_box', 'create_cylinder', 'create_sphere']);
  });

  it('harvests the single `action` field of a capability-level describe', () => {
    expect([...harvestActions({ scope: 'capability', action: 'convert_to_nanite' })])
      .toEqual(['convert_to_nanite']);
  });

  it('harvests objects that name the action under `name` or `action`', () => {
    const found = [...harvestActions({ actions: [{ name: 'a' }, { action: 'b' }, 'c'] })].sort();
    expect(found).toEqual(['a', 'b', 'c']);
  });

  it('finds actions nested at any depth, so a re-shaped envelope does not read as empty', () => {
    expect([...harvestActions({ data: { page: { actions: ['deep'] } } })]).toEqual(['deep']);
  });
});

describe('classify', () => {
  const refusal = (structured: Record<string, unknown>) => ({
    result: { isError: true, structuredContent: { success: false, ...structured } }
  });

  it('names a gateway-level input rejection instead of blaming the engine', () => {
    // The exact refusal the first census provoked by repeating `action` inside
    // `params`. It never reached the plugin, so it is not a gate verdict.
    const seen = classify(refusal({
      errorCode: 'INVALID_PARAMS',
      error: 'params must not override action or subAction. Supply the selected action at the gateway level.'
    }));
    expect(seen.verdict).toBe('REJECTED_INPUT');
    expect(seen.errorCode).toBe('INVALID_PARAMS');
  });

  it('separates a plugin gate from an engine gate', () => {
    expect(classify(refusal({ errorCode: 'PCG_PLUGIN_NOT_ENABLED', error: 'PCG plugin is not enabled' })).verdict)
      .toBe('GATED_PLUGIN');
    expect(classify(refusal({ errorCode: 'UNKNOWN_ACTION', error: 'unknown action' })).verdict)
      .toBe('GATED_ENGINE');
  });

  it('ranks a plugin refusal above the wider engine bucket', () => {
    // GEOMETRYSCRIPTING_PLUGIN_NOT_ENABLED contains neither an engine token nor a
    // NOT_FOUND, but a message may carry both; the plugin reading must win or a
    // disabled optional plugin reads as a 5.7 engine-gate failure.
    const seen = classify(refusal({
      errorCode: 'GEOMETRYSCRIPTING_PLUGIN_NOT_ENABLED',
      error: 'plugin is not enabled; action not found on this engine'
    }));
    expect(seen.verdict).toBe('GATED_PLUGIN');
  });

  it('reads a missing target as proof the handler was REACHED', () => {
    expect(classify(refusal({ errorCode: 'ACTOR_NOT_FOUND', error: 'actor not found' })).verdict)
      .toBe('REACHED');
  });

  it('reads an editor-state refusal as a state gate', () => {
    expect(classify(refusal({ errorCode: 'EDITOR_STATE_UNSUPPORTED', error: 'requires PIE' })).verdict)
      .toBe('GATED_STATE');
  });

  it('calls a genuine success SUCCEEDED', () => {
    expect(classify({ result: { structuredContent: { success: true } } }).verdict).toBe('SUCCEEDED');
  });

  it('returns UNCLEAR rather than inventing a verdict it cannot support', () => {
    const seen = classify(refusal({ errorCode: 'SOMETHING_NEW', error: 'a refusal nobody has classified yet' }));
    expect(seen.verdict).toBe('UNCLEAR');
    // UNCLEAR must never be silently counted as agreement.
    expect(seen.verdict).not.toBe('SUCCEEDED');
  });

  it('treats every caller-error code as the caller\'s fault, not the engine\'s', () => {
    for (const code of CALLER_ERROR_CODES) {
      expect(classify(refusal({ errorCode: code, error: 'x' })).verdict).toBe('REJECTED_INPUT');
    }
  });
});

describe('reachedEditor', () => {
  it('recognises a receipt stamped with the editor\'s live revisions', () => {
    expect(reachedEditor(PLUGIN_REFUSAL)).toBe(true);
  });

  it('does not credit a gateway refusal that never dispatched', () => {
    expect(reachedEditor(GATEWAY_REFUSAL)).toBe(false);
    expect(reachedEditor({})).toBe(false);
  });
});

describe('classify — the same error code, two opposite meanings', () => {
  it('reads a handler-validated refusal as REACHED, proving the capability exists here', () => {
    // UE 5.7.4 refused manage_geometry.convert_to_nanite for a missing actorName.
    // That refusal came FROM the handler, so the 5.7-gated capability is present.
    const seen = classify(PLUGIN_REFUSAL);
    expect(seen.verdict).toBe('REACHED');
    expect(seen.reachedEditor).toBe(true);
  });

  it('reads the pre-dispatch refusal as the caller\'s fault, not the engine\'s', () => {
    const seen = classify(GATEWAY_REFUSAL);
    expect(seen.verdict).toBe('REJECTED_INPUT');
    expect(seen.reachedEditor).toBe(false);
  });

  it('recovers the error code from the text envelope when it is not a structured field', () => {
    expect(classify(PLUGIN_REFUSAL).errorCode).toBe('INVALID_ARGUMENT');
  });
});

describe('censusTool — following the pagination the surface advertises', () => {
  const pagingDriver = (total: number, pageSize: number) => {
    const all = Array.from({ length: total }, (_, index) => `action_${index}`);
    const calls: number[] = [];
    return {
      calls,
      callTool: async (args: { offset?: number }) => {
        const offset = args.offset ?? 0;
        calls.push(offset);
        const page = all.slice(offset, offset + pageSize);
        return {
          response: {
            result: {
              structuredContent: {
                actions: page,
                actionCount: total,
                actionOffset: offset,
                actionHasMore: offset + page.length < total
              }
            }
          }
        };
      }
    };
  };

  it('pages a 76-action tool served 20 at a time and finds every action', async () => {
    // The native surface pages at 20; the first census took page one as the whole
    // list and reported 56 of 76 actions missing on a surface behaving correctly.
    const driver = pagingDriver(76, 20);
    const census = await censusTool(driver, 'manage_geometry');
    expect(census.names).toHaveLength(76);
    expect(census.method).toBe('actions-array-paged');
    expect(driver.calls).toEqual([0, 20, 40, 60]);
  });

  it('stops after one page when the surface returns everything at once', async () => {
    const driver = pagingDriver(76, 76);
    const census = await censusTool(driver, 'manage_geometry');
    expect(census.names).toHaveLength(76);
    expect(driver.calls).toEqual([0]);
  });

  it('falls back to a deep scan and SAYS SO when there is no actions array', async () => {
    const driver = {
      callTool: async () => ({ response: { result: { structuredContent: { data: { actions: ['x'] } } } } })
    };
    const census = await censusTool(driver, 'whatever');
    expect(census.method).toBe('deep-harvest-fallback');
    expect(census.names).toEqual(['x']);
  });

  it('terminates rather than looping when a surface always claims more', async () => {
    const driver = {
      callTool: async () => ({
        response: { result: { structuredContent: { actions: [], actionCount: 999, actionHasMore: true } } }
      })
    };
    const census = await censusTool(driver, 'stuck');
    expect(census.names).toEqual([]);
  });
});

describe('atLeast — the declared engine range', () => {
  const V5_7_4 = { major: 5, minor: 7, patch: 4 };

  it('admits the one capability gated at 5.7.0 on a 5.7.4 engine', () => {
    // manage_geometry.convert_to_nanite is the ONLY record in the catalogue
    // declaring a minimum above the 5.0 baseline. 5.7.4 must satisfy it.
    expect(atLeast(V5_7_4, { major: 5, minor: 7, patch: 0 })).toBe(true);
  });

  it('withholds that same capability on the older certified engines', () => {
    for (const engine of [
      { major: 5, minor: 0, patch: 3 },
      { major: 5, minor: 3, patch: 2 },
      { major: 5, minor: 5, patch: 4 }
    ]) {
      expect(atLeast(engine, { major: 5, minor: 7, patch: 0 })).toBe(false);
    }
  });

  it('does not exceed the declared 5.8.0 maximum on 5.7.4', () => {
    expect(atLeast({ major: 5, minor: 8, patch: 0 }, V5_7_4)).toBe(true);
  });

  it('compares patch levels rather than stopping at the minor', () => {
    expect(atLeast({ major: 5, minor: 7, patch: 0 }, { major: 5, minor: 7, patch: 4 })).toBe(false);
    expect(atLeast({ major: 5, minor: 7, patch: 4 }, { major: 5, minor: 7, patch: 4 })).toBe(true);
  });
});
