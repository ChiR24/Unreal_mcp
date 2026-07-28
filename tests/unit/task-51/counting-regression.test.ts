// tests/unit/task-51/counting-regression.test.ts
// Task 51 — regressions for the two counting defects that made the load and soak
// gates impossible to fail.
//
// D1 — THE LOAD COUNTED PLANNED REQUESTS AS ISSUED ONES. `runLoad` incremented one
// `issued` counter immediately after every `callTool`, and the plan loop has no
// early exit, so `requestsIssued` was always exactly `spec.requests`. The gate
// `requestsIssued === options.requests` therefore compared the plan's length with
// itself: a run in which all 1,000 requests timed out, or in which every `execute`
// unexpectedly SUCCEEDED with no bridge attached, produced the same green tick as a
// perfect run.
//
// D3 — THE SOAK COUNTED CYCLES IT NEVER RAN, AND CYCLED NOTHING. `runSoak` returned
// `cycles: spec.cycles` — the input, echoed — and `cleanupCycle` called
// `enable_tool` / `disable_tool`, actions manage_tools does not have (they are
// `enable_tools` / `disable_tools`, taking `tools: []` under `params`). Every cycle
// of the recorded 500-cycle run answered `success:false / UNKNOWN_ACTION` twice,
// and the cycle only checked for a TIMEOUT, so it reported ok. A cleanup soak that
// never creates state cannot observe a faulty teardown; the gate then read only
// `failures.length === 0`, which an unstarted soak also satisfies.
//
// Every test below drives the real harness functions through a scripted driver, so
// the assertions are about the shipped counting rules rather than a restatement of
// them.

import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { runLoad, isExpectedVerdict, EXPECTED_VERDICT } from './load-harness.mjs';
import { cleanupCycle, configureChangedState, readGatewayEnvelope, runSoak, runProcessResidueSoak, SOAK_TOOLS } from './soak-harness.mjs';

/** Above the default `kernel.pid_max`, so it can never name a live process. */
const ABSENT_PID = 4_194_305;

/** Keep the harnesses' RSS sampling and drain out of the test clock. */
const FAST = { rssSettle: { samples: 1, windowMs: 0 }, drainMs: 0 } as const;

type Frame = Record<string, any> | null;
type Responder = (args: Record<string, any>, index: number) => Frame;

/** A driver that answers from a script and records exactly what it was asked. */
class ScriptedDriver {
  calls: Record<string, any>[] = [];
  notifications: unknown[] = [];
  decoder = { malformed: 0 };
  closeCount = 0;

  constructor(
    private readonly respond: Responder,
    private readonly startResult: { ok: boolean; reason: string; pid: number | null } = { ok: true, reason: 'READY', pid: ABSENT_PID },
  ) {}

  async start() { return this.startResult; }

  async callTool(args: Record<string, any>) {
    const index = this.calls.length;
    this.calls.push(args);
    const response = this.respond(args, index);
    return {
      requestId: index,
      status: response === null ? 0 : 200,
      body: response === null ? 'TIMEOUT' : JSON.stringify(response),
      response,
      streamNotifications: [],
      frameCount: response === null ? 0 : 1,
      ms: 0,
    };
  }

  async close() { this.closeCount += 1; return { stopped: true, pid: null, signal: null, alreadyClosed: false }; }
  verifyChildReleased() { return { released: true, observed: 'scripted driver owns no child' }; }
  actions() { return this.calls.map((call) => String(call.action ?? call.operation)); }
}

/** The shape `wrapResponse('unreal', …)` puts on the wire: envelope in structuredContent. */
function frame(payload: Record<string, unknown>): Frame {
  return { jsonrpc: '2.0', id: 1, result: { content: [{ type: 'text', text: JSON.stringify(payload) }], structuredContent: payload } };
}

function configureOk(action: string, listKey: 'enabled' | 'disabled', tools: string[]): Frame {
  return frame({ success: true, operation: 'configure', action, result: { success: true, [listKey]: tools, notFound: [], message: 'ok' } });
}

/** What manage_tools really answers for a singular `enable_tool`. */
function unknownAction(action: string): Frame {
  return frame({
    success: false, operation: 'configure', action,
    result: { success: false, error: `Unknown action: ${action}. Available: list_tools, ...`, errorCode: 'UNKNOWN_ACTION' },
  });
}

const describeOk = (): Frame => frame({ success: true, operation: 'describe', tool: 'manage_geometry' });

/** A driver that answers every load request exactly as its kind requires. */
const perfectLoadResponder: Responder = (args) => {
  if (args.operation === 'execute') {
    return args.tool === 'no_such_tool'
      ? frame({ success: false, operation: 'execute', errorCode: 'UNKNOWN_CAPABILITY' })
      : frame({ success: false, operation: 'execute', errorCode: 'NOT_CONNECTED' });
  }
  return frame({ success: true, operation: String(args.operation) });
};

describe('Task 51 D1 — the load gate counts answers, not the plan it was handed', () => {
  it('declares one required verdict per kind and scores an unknown kind as never succeeded', () => {
    expect(EXPECTED_VERDICT['execute-unconnected']).toBe('FAILED_CLOSED');
    expect(EXPECTED_VERDICT['execute-unknown']).toBe('REFUSED');
    expect(isExpectedVerdict('search', 'OK')).toBe(true);
    expect(isExpectedVerdict('search', 'TIMEOUT')).toBe(false);
    // A request kind nobody has decided an expectation for cannot be counted as a
    // success just because a frame came back.
    expect(isExpectedVerdict('kind-nobody-declared', 'OK')).toBe(false);
  });

  it('counts every planned request as succeeded when the server answers correctly (the positive control)', async () => {
    const result = await runLoad({
      sessions: 2, requests: 12, seed: 'd1-positive', ...FAST,
      driverFactory: () => new ScriptedDriver(perfectLoadResponder),
    });
    expect(result.started).toBe(2);
    expect(result.requestsPlanned).toBe(12);
    expect(result.requestsAttempted).toBe(12);
    expect(result.requestsAnswered).toBe(12);
    expect(result.requestsSucceeded).toBe(12);
  });

  it('drops succeeded and answered to zero when every request times out', async () => {
    // The defect in one line: this run does nothing at all, and the old counter
    // still reported 12 of 12 issued.
    const result = await runLoad({
      sessions: 1, requests: 12, seed: 'd1-timeout', ...FAST,
      driverFactory: () => new ScriptedDriver(() => null),
    });
    expect(result.requestsAttempted).toBe(12);
    expect(result.requestsAnswered).toBe(0);
    expect(result.requestsSucceeded).toBe(0);
    expect(result.requestsSucceeded).not.toBe(result.requestsPlanned);
  });

  it('does not credit an execute that unexpectedly SUCCEEDED with no bridge attached', async () => {
    const result = await runLoad({
      sessions: 1, requests: 40, seed: 'd1-unexpected', ...FAST,
      driverFactory: () => new ScriptedDriver((args) => frame({ success: true, operation: String(args.operation) })),
    });
    // Everything was answered, so a gate on "answered" alone would pass here.
    expect(result.requestsAnswered).toBe(40);
    // But the refusal paths returned success, which must never score.
    expect(result.requestsSucceeded).toBeLessThan(result.requestsPlanned);
    const unexpected = Object.entries(result.outcomes).filter(([key]) => key.endsWith(':UNEXPECTED'));
    expect(unexpected.length).toBeGreaterThan(0);
  });

  it('reports a session that never initialized as unstarted rather than as load', async () => {
    const result = await runLoad({
      sessions: 3, requests: 9, seed: 'd1-nostart', ...FAST,
      driverFactory: () => new ScriptedDriver(perfectLoadResponder, { ok: false, reason: 'INITIALIZE_ERROR', pid: ABSENT_PID }),
    });
    expect(result.started).toBe(0);
    expect(result.sessionsPlanned).toBe(3);
    expect(result.startFailures).toHaveLength(3);
    expect(result.requestsAttempted).toBe(0);
    expect(result.requestsSucceeded).toBe(0);
    // The planned figure is still reported, so the gate has something to fail against.
    expect(result.requestsPlanned).toBe(9);
  });
});

describe('Task 51 D3 — a cycle counts only if it opened, and closes only what it opened', () => {
  it('uses the actions manage_tools actually has, with the tool under params', async () => {
    const driver = new ScriptedDriver((args) => {
      if (args.action === 'enable_tools') return configureOk('enable_tools', 'enabled', ['manage_geometry']);
      if (args.action === 'disable_tools') return configureOk('disable_tools', 'disabled', ['manage_geometry']);
      return describeOk();
    });
    const outcome = await cleanupCycle(driver as never, 'manage_geometry', 1000);
    expect(outcome).toMatchObject({ ok: true, opened: true, used: true, closed: true, listConfirmed: true, leakedOpenState: false });
    expect(driver.actions()).toEqual(['enable_tools', 'describe', 'disable_tools']);
    // The singular spellings are what the defect used; they must not come back.
    expect(driver.actions()).not.toContain('enable_tool');
    expect(driver.calls[0].params).toEqual({ tools: ['manage_geometry'] });
    expect(driver.calls[2].params).toEqual({ tools: ['manage_geometry'] });
  });

  it('does not send a teardown for state the enable never created', async () => {
    // This is the recorded 500-cycle run: UNKNOWN_ACTION, twice, 500 times.
    const driver = new ScriptedDriver((args) => (args.operation === 'configure' ? unknownAction(String(args.action)) : describeOk()));
    const outcome = await cleanupCycle(driver as never, 'manage_geometry', 1000);
    expect(outcome.opened).toBe(false);
    expect(outcome.ok).toBe(false);
    expect(outcome.stage).toBe('enable');
    expect(outcome.detail).toContain('UNKNOWN_ACTION');
    // Fabricating a disable here is exactly what painted the old run green.
    expect(driver.actions()).toEqual(['enable_tools']);
  });

  it('still closes the capability when the middle step fails, so later cycles are not contaminated', async () => {
    const driver = new ScriptedDriver((args) => {
      if (args.action === 'enable_tools') return configureOk('enable_tools', 'enabled', ['manage_pcg']);
      if (args.action === 'disable_tools') return configureOk('disable_tools', 'disabled', ['manage_pcg']);
      return null; // the describe times out
    });
    const outcome = await cleanupCycle(driver as never, 'manage_pcg', 1000);
    expect(outcome.opened).toBe(true);
    expect(outcome.used).toBe(false);
    expect(outcome.closed).toBe(true);
    expect(outcome.ok).toBe(false);
    expect(outcome.leakedOpenState).toBe(false);
    expect(driver.actions()).toContain('disable_tools');
  });

  it('names an opened-and-not-closed cycle as a leak rather than one more failure line', async () => {
    const driver = new ScriptedDriver((args) => {
      if (args.action === 'enable_tools') return configureOk('enable_tools', 'enabled', ['manage_audio']);
      if (args.action === 'disable_tools') return null;
      return describeOk();
    });
    const outcome = await cleanupCycle(driver as never, 'manage_audio', 1000);
    expect(outcome.leakedOpenState).toBe(true);
    expect(outcome.closed).toBe(false);
    expect(outcome.ok).toBe(false);
  });

  it('refuses to read a tool that landed in notFound as a state change', () => {
    const answered = frame({ success: true, operation: 'configure', action: 'enable_tools', result: { success: true, enabled: [], notFound: ['manage_ai'] } });
    expect(configureChangedState(answered, 'manage_ai', 'enabled').changed).toBe(false);
    expect(configureChangedState(unknownAction('enable_tool'), 'manage_ai', 'enabled')).toMatchObject({ changed: false });
    expect(configureChangedState(null, 'manage_ai', 'enabled').detail).toBe('timed out');
    // An answer nobody can interpret fails closed instead of passing quietly.
    expect(configureChangedState({ result: {} }, 'manage_ai', 'enabled').changed).toBe(false);
    expect(readGatewayEnvelope({ error: { code: -32_600 } })).toBeNull();
    // Content-only clients still get read.
    const contentOnly = { result: { content: [{ type: 'text', text: JSON.stringify({ success: true, result: { enabled: ['manage_ai'] } }) }] } };
    expect(configureChangedState(contentOnly, 'manage_ai', 'enabled').changed).toBe(true);
  });
});

describe('Task 51 D3 — the soak reports cycles it completed, not cycles it planned', () => {
  const workingSoakResponder: Responder = (args) => {
    if (args.action === 'enable_tools') return configureOk('enable_tools', 'enabled', (args.params as any).tools);
    if (args.action === 'disable_tools') return configureOk('disable_tools', 'disabled', (args.params as any).tools);
    return describeOk();
  };

  it('completes every planned cycle when the server behaves (the positive control)', async () => {
    const result = await runSoak({
      cycles: 8, seed: 'd3-positive', warmupCycles: 1, ...FAST,
      driverFactory: () => new ScriptedDriver(workingSoakResponder),
    });
    expect(result.started).toBe(true);
    expect(result.cyclesPlanned).toBe(8);
    expect(result.cyclesAttempted).toBe(8);
    expect(result.cyclesOpened).toBe(8);
    expect(result.cyclesCompleted).toBe(8);
    expect(result.openStateLeaks).toBe(0);
    expect(result.failures).toEqual([]);
  });

  it('reports zero completed cycles — and stops early — when no cycle can open anything', async () => {
    const driver = new ScriptedDriver((args) => (args.operation === 'configure' ? unknownAction(String(args.action)) : describeOk()));
    const result = await runSoak({
      cycles: 500, seed: 'd3-unknown-action', warmupCycles: 2, ...FAST,
      driverFactory: () => driver,
    });
    expect(result.cyclesCompleted).toBe(0);
    expect(result.cyclesOpened).toBe(0);
    expect(result.cyclesPlanned).toBe(500);
    expect(result.blocked).toMatchObject({ code: 'CYCLE_NEVER_OPENED' });
    // It must not burn 500 cycles restating the same non-measurement.
    expect(result.cyclesAttempted).toBe(2);
    expect(driver.actions().filter((action) => action === 'disable_tools')).toEqual([]);
  });

  it('separates completed from attempted when only some cycles close', async () => {
    let enables = 0;
    const result = await runSoak({
      cycles: 6, seed: 'd3-partial', warmupCycles: 1, ...FAST,
      driverFactory: () => new ScriptedDriver((args) => {
        if (args.action === 'enable_tools') { enables += 1; return configureOk('enable_tools', 'enabled', (args.params as any).tools); }
        // Every third teardown fails, leaving the capability enabled.
        if (args.action === 'disable_tools') return enables % 3 === 0 ? null : configureOk('disable_tools', 'disabled', (args.params as any).tools);
        return describeOk();
      }),
    });
    expect(result.cyclesAttempted).toBe(6);
    expect(result.cyclesOpened).toBe(6);
    expect(result.cyclesCompleted).toBeLessThan(6);
    expect(result.openStateLeaks).toBeGreaterThan(0);
    expect(result.failures.length).toBe(6 - (result.cyclesCompleted as number));
  });

  it('closes a session that failed to initialize and credits it with no cycles', async () => {
    const driver = new ScriptedDriver(workingSoakResponder, { ok: false, reason: 'INITIALIZE_TIMEOUT', pid: ABSENT_PID });
    const result = await runSoak({ cycles: 100, seed: 'd3-nostart', warmupCycles: 1, ...FAST, driverFactory: () => driver });
    expect(result.started).toBe(false);
    expect(result.cyclesCompleted).toBe(0);
    expect(result.cyclesAttempted).toBe(0);
    expect(result.cyclesPlanned).toBe(100);
    expect(driver.closeCount).toBe(1);
  });

  it('cycles only unprotected capabilities, so a teardown is a real state change', () => {
    for (const tool of SOAK_TOOLS) {
      expect(['manage_tools', 'inspect']).not.toContain(tool);
    }
  });
});

describe('Task 51 D3 — the residue soak counts rounds it opened', () => {
  it('does not count a round whose session never initialized, and closes it anyway', async () => {
    const drivers: ScriptedDriver[] = [];
    const result = await runProcessResidueSoak({
      rounds: 3,
      driverFactory: () => {
        const driver = new ScriptedDriver(
          () => frame({ success: true, operation: 'search' }),
          drivers.length === 0 ? { ok: false, reason: 'INITIALIZE_ERROR', pid: ABSENT_PID } : { ok: true, reason: 'READY', pid: ABSENT_PID },
        );
        drivers.push(driver);
        return driver;
      },
    });
    expect(result.roundsPlanned).toBe(3);
    expect(result.roundsOpened).toBe(2);
    expect(result.roundsCompleted).toBe(2);
    expect(result.residue.some((entry: string) => entry.includes('never initialized'))).toBe(true);
    // Every driver constructed is torn down, including the one that never opened.
    expect(drivers.map((driver) => driver.closeCount)).toEqual([1, 1, 1]);
  });

  it('closes a driver whose spawn never produced a pid instead of dropping it', async () => {
    const drivers: ScriptedDriver[] = [];
    const result = await runProcessResidueSoak({
      rounds: 2,
      driverFactory: () => {
        const driver = new ScriptedDriver(() => null, { ok: false, reason: 'SPAWN_FAILED', pid: null });
        drivers.push(driver);
        return driver;
      },
    });
    expect(result.roundsOpened).toBe(0);
    expect(result.roundsCompleted).toBe(0);
    expect(drivers.every((driver) => driver.closeCount === 1)).toBe(true);
  });
});

describe('Task 51 — the adversarial gate table reads the measured counts', () => {
  const source = readFileSync(resolve(process.cwd(), 'scripts/qa/task51-adversarial.mjs'), 'utf8');

  it('no longer gates on a counter that equals the plan by construction', () => {
    expect(source).not.toContain('allRequestsIssued');
    // The counter itself must be unreachable, not merely unnamed by a gate. (The
    // word still appears in the comment that explains why it is gone.)
    expect(source).not.toContain('load.requestsIssued');
  });

  it('gates the load on answered and succeeded counts', () => {
    expect(source).toContain('allRequestsAnswered');
    expect(source).toContain('allRequestsSucceeded');
    expect(source).toContain('load.requestsSucceeded === options.requests');
  });

  it('gates the soak on a started session, completed cycles and zero open-state leaks', () => {
    expect(source).toContain('soak.started === true');
    expect(source).toContain('(soak.cyclesCompleted ?? 0) === options.cycles');
    expect(source).toContain('(soak.openStateLeaks ?? 0) === 0');
  });
});
