// tests/unit/live-drivers/runner-oracle.test.ts
// Task 49 — the independent-oracle and cleanup semantics, pinned offline.
//
// Every case here exists because run 1 of the live corpus got it WRONG:
//
//   - it reported `cleanupClean: true` while two materials sat on disk, because
//     every `asset.delete_asset` had answered INVALID_ARGUMENT and the step was
//     marked `tolerateFailure`
//   - the stdio side's oracle read `present` and PASSED its oracle while its own
//     create had errored — it was reading the asset the NATIVE side had created
//     and failed to delete
//
// Both were caught by an independent filesystem check, not by the harness. These
// tests are what stop them coming back.

import { describe, expect, it } from 'vitest';

import {
  classifyOutcome,
  compileRequest,
  consultOracle,
  judge,
  legacyArgs,
  runScenario,
} from './live-corpus-runner.mjs';
import { indexRecords, validateScenario } from './live-corpus-schema.mjs';
import { loadRecords } from '../cross-transport/matrix-dimensions.mjs';

const index = indexRecords(loadRecords());
const OWNED = '/Game/MCPTest/task49-runner-spec';

function mutatingScenario() {
  return validateScenario({
    namespace: 'task49.runner.spec',
    title: 'runner spec fixture',
    primitive: 'execute',
    form: 'canonical',
    capability: 'material.create_material',
    ownedPath: OWNED,
    request: {
      params: { name: 'M_RunnerSpec', path: OWNED },
      consent: { capability: 'material.create_material', acknowledge: 'explicit' },
    },
    expected: 'success',
    timeoutTier: 'interactive',
    oracle: { capability: 'asset.list', params: { path: OWNED }, expect: 'present', needle: 'M_RunnerSpec', attempts: 2, intervalMs: 0 },
    cleanup: [{
      capability: 'asset.delete_asset',
      params: { paths: [`${OWNED}/M_RunnerSpec`] },
      consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' },
      tolerateFailure: true,
    }],
    requires: { unrealMin: '5.0.0', plugins: ['EditorScriptingUtilities'], editorStates: ['edit'], clients: ['stdio'] },
  }, { index });
}

/** A driver whose editor is a set of asset names, so the oracle reads real state. */
class FakeDriver {
  name = 'fake';
  kind = 'stdio';
  rpcId = 1;
  notifications: unknown[] = [];
  calls: Record<string, unknown>[] = [];

  constructor(
    private readonly world: Set<string>,
    private readonly behavior: {
      createSucceeds?: boolean;
      createMutatesAnyway?: boolean;
      deleteSucceeds?: boolean;
    } = {},
  ) {}

  private ok(payload: Record<string, unknown>) {
    this.rpcId += 1;
    return {
      requestId: this.rpcId,
      status: 200,
      body: JSON.stringify(payload),
      response: { jsonrpc: '2.0', id: this.rpcId, result: { structuredContent: payload } },
      streamNotifications: [],
      frameCount: 1,
      ms: 1,
    };
  }

  async callTool(args: Record<string, unknown>) {
    this.calls.push(args);
    // Resolve BOTH request forms. The canonical form carries no `action` at all,
    // so a fake that only reads `args.action` silently answers "success, changed
    // nothing" to every canonical call.
    const action = String(args.action ?? String(args.capability ?? '').split('.')[1] ?? '');
    if (action === 'list') {
      return this.ok({ success: true, data: { assets: [...this.world].map((name) => ({ name })), folders: [OWNED] } });
    }
    if (action === 'create_material') {
      if (this.behavior.createSucceeds === false) {
        // The exact live shape: the call reports failure. Whether it ALSO mutated
        // is the interesting axis — a response is not evidence either way.
        if (this.behavior.createMutatesAnyway) this.world.add('M_RunnerSpec');
        return this.ok({ success: false, errorCode: 'UNREAL_EXECUTION_ERROR', message: 'create failed' });
      }
      this.world.add('M_RunnerSpec');
      return this.ok({ success: true, message: 'Material created' });
    }
    if (action === 'delete_asset') {
      if (this.behavior.deleteSucceeds === false) {
        return this.ok({ success: false, errorCode: 'INVALID_ARGUMENT', message: 'No paths provided' });
      }
      this.world.delete('M_RunnerSpec');
      return this.ok({ success: true, message: 'Assets deleted' });
    }
    return this.ok({ success: true });
  }

  async cancel() { return { sent: true, status: 200, requestId: 0, reason: '' }; }
}

describe('runner — the oracle is independent and is read, not assumed', () => {
  it('passes a real mutation: absent before, present after, gone after cleanup', async () => {
    const world = new Set<string>();
    const driver = new FakeDriver(world);
    const row = await runScenario(driver, mutatingScenario());
    expect(row.preState.verdict).toBe(false);
    expect(row.oracle.pass).toBe(true);
    expect(row.cleanupVerified).toBe(true);
    expect(row.status).toBe('PASS');
    expect(world.size).toBe(0);
  });

  it('BLOCKS when the fixture already exists, instead of reading a leftover as proof', async () => {
    // This is precisely run 1's stdio failure: native had created M_Task49Canonical
    // and failed to delete it, so stdio's oracle saw `present` for an asset stdio
    // had not created.
    const driver = new FakeDriver(new Set(['M_RunnerSpec']));
    const row = await runScenario(driver, mutatingScenario());
    expect(row.status).toBe('BLOCKED');
    expect(row.detail).toContain('already existed');
    expect(row.oracle).toBeUndefined();
  });

  it('FAILS when cleanup reports success but the asset survives', async () => {
    const world = new Set<string>();
    const driver = new FakeDriver(world, { deleteSucceeds: false });
    const row = await runScenario(driver, mutatingScenario());
    expect(row.oracle.pass).toBe(true);
    expect(row.cleanup[0].errorCode).toBe('INVALID_ARGUMENT');
    expect(row.cleanupVerified).toBe(false);
    expect(row.status).toBe('FAIL');
  });

  it('does not let a tolerated cleanup failure hide a leak', async () => {
    const scenario = mutatingScenario();
    expect(scenario.cleanup[0].tolerateFailure).toBe(true);
    const row = await runScenario(new FakeDriver(new Set(), { deleteSucceeds: false }), scenario);
    expect(row.status).toBe('FAIL');
  });

  it('FAILS a call that reported success when the oracle cannot see the asset', async () => {
    // A forged success: the response says created, the world disagrees.
    const driver = new FakeDriver(new Set());
    driver.callTool = (async (args: Record<string, unknown>) => {
      if (String(args.action) === 'list') {
        return {
          requestId: 1, status: 200, streamNotifications: [], frameCount: 1, ms: 1,
          body: '{"success":true}',
          response: { result: { structuredContent: { success: true, data: { assets: [], folders: [] } } } },
        };
      }
      return {
        requestId: 1, status: 200, streamNotifications: [], frameCount: 1, ms: 1,
        body: '{"success":true}',
        response: { result: { structuredContent: { success: true, message: 'Material created' } } },
      };
    }) as never;
    const row = await runScenario(driver, mutatingScenario());
    expect(row.judgement.pass).toBe(true);
    expect(row.oracle.pass).toBe(false);
    expect(row.status).toBe('FAIL');
  });

  it('reports an unusable oracle as inconclusive rather than as a convenient negative', async () => {
    const scenario = mutatingScenario();
    const driver = {
      name: 'blind',
      async callTool() {
        return { requestId: 1, status: 200, body: 'nope', response: { result: {} }, streamNotifications: [], frameCount: 1, ms: 1 };
      },
    };
    const verdict = await consultOracle(driver, scenario, 1000);
    expect(verdict.verdict).toBeNull();
    expect(verdict.conclusive).toBe(false);
    expect(verdict.pass).toBe(false);
  });
});

describe('runner — request compilation puts the declared form on the wire', () => {
  it('canonical form sends `capability` and never tool/action', () => {
    const args = compileRequest(mutatingScenario());
    expect(args.capability).toBe('material.create_material');
    expect(args).not.toHaveProperty('tool');
    expect(args).not.toHaveProperty('action');
  });

  it('legacy form sends tool + action and never `capability`', () => {
    const scenario = { ...mutatingScenario(), form: 'legacy' };
    const args = compileRequest(scenario);
    expect(args.tool).toBe('manage_asset');
    expect(args.action).toBe('create_material');
    expect(args).not.toHaveProperty('capability');
  });

  it('scaffolding always uses the legacy form so a form defect cannot break the oracle', () => {
    const record = index.get('asset.list');
    const args = legacyArgs(record, { path: '/Game' }, null);
    expect(args).toEqual({ operation: 'execute', tool: 'manage_asset', action: 'list', params: { path: '/Game' } });
  });
});

describe('runner — outcome classification and judgement', () => {
  const observed = (payload: Record<string, unknown>) => ({
    status: 200,
    body: JSON.stringify(payload),
    response: { result: { structuredContent: payload } },
  });

  it('reads a typed refusal as an error carrying its code', () => {
    const outcome = classifyOutcome(observed({ success: false, errorCode: 'CONSENT_REQUIRED' }));
    expect(outcome.intent).toBe('error');
    expect(outcome.errorCode).toBe('CONSENT_REQUIRED');
  });

  it('reads a missing response as a timeout, not as an error', () => {
    expect(classifyOutcome({ status: 0, body: 'TIMEOUT', response: null }).intent).toBe('timeout');
  });

  it('refuses an error whose code is not the one the scenario named', () => {
    const scenario = validateScenario({
      ...mutatingScenario0(), expected: 'error', expectedErrorCode: 'CONSENT_REQUIRED',
    }, { index });
    const verdict = judge(scenario, classifyOutcome(observed({ success: false, errorCode: 'UNREAL_EXECUTION_ERROR' })));
    expect(verdict.pass).toBe(false);
    expect(verdict.reason).toContain('CONSENT_REQUIRED');
  });

  it('accepts a narrow success alternative but not an arbitrary error', () => {
    const scenario = validateScenario({ ...mutatingScenario0(), expected: 'success|already exists' }, { index });
    expect(judge(scenario, classifyOutcome(observed({ success: false, message: 'Asset already exists' }))).pass).toBe(true);
    expect(judge(scenario, classifyOutcome(observed({ success: false, message: 'disk on fire' }))).pass).toBe(false);
  });
});

/** The raw (pre-validation) form of the fixture, for cases that vary one field. */
function mutatingScenario0(): Record<string, unknown> {
  return {
    namespace: 'task49.runner.spec',
    title: 'runner spec fixture',
    primitive: 'execute',
    form: 'canonical',
    capability: 'material.create_material',
    ownedPath: OWNED,
    request: {
      params: { name: 'M_RunnerSpec', path: OWNED },
      consent: { capability: 'material.create_material', acknowledge: 'explicit' },
    },
    expected: 'success',
    timeoutTier: 'interactive',
    oracle: { capability: 'asset.list', params: { path: OWNED }, expect: 'present', needle: 'M_RunnerSpec', attempts: 2, intervalMs: 0 },
    cleanup: [{
      capability: 'asset.delete_asset',
      params: { paths: [`${OWNED}/M_RunnerSpec`] },
      consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' },
      tolerateFailure: true,
    }],
    requires: { unrealMin: '5.0.0', plugins: ['EditorScriptingUtilities'], editorStates: ['edit'], clients: ['stdio'] },
  };
}
