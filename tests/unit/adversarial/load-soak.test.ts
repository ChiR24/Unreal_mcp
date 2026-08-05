// tests/unit/adversarial/load-soak.test.ts
// Task 51 — the offline half of the load and soak harnesses.
//
// The live run is in scripts/qa/adversarial.mjs and needs 32 spawned
// children; these are the tests that make its numbers mean something without
// spawning anything but one tiny helper process.
//
// The important one is the RSS POSITIVE CONTROL. A retained-memory gate that has
// never been shown to detect a leak is decoration: it reports a number, the number
// is under the limit, and nobody can say whether that is because the code is clean
// or because the measurement is blind. The first pilot run of this harness produced
// a retained figure of MINUS 53 MB — a trivially passing gate caused by sampling the
// baseline at a post-warm-up peak. So this file proves the corrected sampler sees a
// deliberate leak, and that the leak-free control does not move.

import { spawn } from 'node:child_process';
import { describe, expect, it } from 'vitest';

import { buildRequestPlan, classifyResponse, loadEnv, readRssBytes, sampleSettledRss, summariseRss, OWNED_WS_PORTS, processAlive } from './load-harness.mjs';
import { SOAK_TOOLS } from './soak-harness.mjs';
import { SEEDS, BUDGETS, RSS_LIMITS } from './fuzz-seeds.mjs';

const onLinux = process.platform === 'linux';

/**
 * A child that allocates `mb` of genuinely retained memory (held in a module-scope
 * array so it cannot be collected), reports ready, and then idles until killed.
 */
function spawnBallastChild(mb: number) {
  const source = `
    const held = [];
    process.stdout.write('ready\\n');
    process.stdin.on('data', () => {
      for (let i = 0; i < ${mb}; i += 1) held.push(Buffer.alloc(1024 * 1024, i % 251));
      process.stdout.write('allocated\\n');
    });
    setInterval(() => {}, 1000);
  `;
  return spawn(process.execPath, ['-e', source], { stdio: ['pipe', 'pipe', 'pipe'] });
}

/** Wait for one line of stdout matching `token`. */
function waitForLine(child: ReturnType<typeof spawn>, token: string, timeoutMs = 15_000) {
  return new Promise<boolean>((settle) => {
    const timer = setTimeout(() => settle(false), timeoutMs);
    const onData = (chunk: Buffer) => {
      if (String(chunk).includes(token)) {
        clearTimeout(timer);
        child.stdout?.off('data', onData);
        settle(true);
      }
    };
    child.stdout?.on('data', onData);
  });
}

describe('Task 51 — the retained-RSS measurement can actually detect a leak', () => {
  it.runIf(onLinux)('reports growth when a child deliberately retains 64 MB', async () => {
    const child = spawnBallastChild(64);
    try {
      expect(await waitForLine(child, 'ready')).toBe(true);
      const pid = child.pid as number;
      const baseline = await sampleSettledRss(pid, { samples: 4, windowMs: 800 });
      expect(baseline.min).not.toBeNull();

      child.stdin?.write('go\n');
      expect(await waitForLine(child, 'allocated')).toBe(true);
      const after = await sampleSettledRss(pid, { samples: 4, windowMs: 800 });

      const retained = (after.min as number) - (baseline.min as number);
      // The gate is 32 MiB; a 64 MB leak must land clearly on the failing side of
      // it, or the gate would pass a leak twice its own size.
      expect(retained).toBeGreaterThan(RSS_LIMITS.nodeRetainedBytes);
    } finally {
      child.kill('SIGKILL');
      await new Promise((settle) => { child.once('exit', settle); });
    }
  }, 60_000);

  it.runIf(onLinux)('reports no meaningful growth for an idle child (the negative control)', async () => {
    const child = spawnBallastChild(0);
    try {
      expect(await waitForLine(child, 'ready')).toBe(true);
      const pid = child.pid as number;
      const baseline = await sampleSettledRss(pid, { samples: 4, windowMs: 800 });
      const after = await sampleSettledRss(pid, { samples: 4, windowMs: 800 });
      const retained = (after.min as number) - (baseline.min as number);
      expect(retained).toBeLessThan(RSS_LIMITS.nodeRetainedBytes);
    } finally {
      child.kill('SIGKILL');
      await new Promise((settle) => { child.once('exit', settle); });
    }
  }, 60_000);

  it.runIf(onLinux)('returns a null reading rather than a zero for a pid that is gone', async () => {
    const child = spawnBallastChild(0);
    expect(await waitForLine(child, 'ready')).toBe(true);
    const pid = child.pid as number;
    expect(readRssBytes(pid)).not.toBeNull();
    child.kill('SIGKILL');
    await new Promise((settle) => { child.once('exit', settle); });
    // A zero here would read as "used no memory" instead of "was not there".
    expect(readRssBytes(pid)).toBeNull();
    expect(processAlive(pid)).toBe(false);
  }, 60_000);

  // The RSS reader is /proc-based, so the sampler itself is Linux-only; the
  // sibling controls are gated the same way.
  it.runIf(onLinux)('takes the settled trough, not the instantaneous sample', async () => {
    const settled = await sampleSettledRss(process.pid, { samples: 4, windowMs: 200 });
    expect(settled.samples).toBe(4);
    expect(settled.min).toBeLessThanOrEqual(settled.max as number);
  });
});

describe('Task 51 — D4: a retained-RSS comparison that cannot fail is not scored as a pass', () => {
  // The live 32-session run produced a retained delta on EVERY session between
  // -28.61 MiB and -148.09 MiB, with peakOverBaseline negative throughout. Against a
  // `<= 32 MiB` limit that is true for any input a leak included, so the settled-trough
  // fix had not removed the unfalsifiable shape, only moved it from one process to 32.
  const MiB = 1024 * 1024;

  it('flags the baseline as a peak when no session ever rose above it', () => {
    const summary = summariseRss([
      { retainedBytes: -30 * MiB, peakOverBaselineBytes: -28 * MiB, secondHalfGrowthBytes: 1 * MiB },
      { retainedBytes: -148 * MiB, peakOverBaselineBytes: -140 * MiB, secondHalfGrowthBytes: 2 * MiB },
    ]);
    expect(summary.baselineWasAPeak).toBe(true);
    // The comparison itself still "passes" — which is exactly why passing it proves nothing.
    expect(summary.worstRetainedBytes as number).toBeLessThan(32 * MiB);
  });

  it('does not flag a baseline that the workload genuinely rose above', () => {
    const summary = summariseRss([
      { retainedBytes: 4 * MiB, peakOverBaselineBytes: 12 * MiB, secondHalfGrowthBytes: 1 * MiB },
      { retainedBytes: -2 * MiB, peakOverBaselineBytes: 9 * MiB, secondHalfGrowthBytes: 0 },
    ]);
    expect(summary.baselineWasAPeak).toBe(false);
  });

  it('reports the WORST second-half growth, so one leaking session fails the run', () => {
    const summary = summariseRss([
      { peakOverBaselineBytes: 5 * MiB, secondHalfGrowthBytes: 1 * MiB },
      { peakOverBaselineBytes: 5 * MiB, secondHalfGrowthBytes: 64 * MiB },
      { peakOverBaselineBytes: 5 * MiB, secondHalfGrowthBytes: 0 },
    ]);
    // Averaging would hide this at ~21 MiB and pass the 32 MiB gate.
    expect(summary.worstGrowthBytes).toBe(64 * MiB);
    expect(summary.worstGrowthBytes as number).toBeGreaterThan(RSS_LIMITS.nodeRetainedBytes);
    expect(summary.growthCount).toBe(3);
  });

  it('counts sessions that produced no figure, so a missing reading cannot pass by absence', () => {
    const summary = summariseRss([
      { peakOverBaselineBytes: 5 * MiB, secondHalfGrowthBytes: 1 * MiB },
      { peakOverBaselineBytes: 5 * MiB, secondHalfGrowthBytes: null },
    ]);
    // The runner requires growthCount === sessions.length, so this run cannot be green.
    expect(summary.growthCount).toBe(1);
  });
});

describe('Task 51 — the load plan is deterministic and never touches a foreign resource', () => {
  it('produces byte-identical plans for the pinned seed', () => {
    const first = buildRequestPlan(SEEDS.load, 1000);
    const second = buildRequestPlan(SEEDS.load, 1000);
    expect(JSON.stringify(second)).toBe(JSON.stringify(first));
    expect(first).toHaveLength(BUDGETS.loadRequests);
  });

  it('mixes reads with refusal paths rather than issuing one request type', () => {
    const kinds = new Set(buildRequestPlan(SEEDS.load, 1000).map((entry) => entry.kind));
    expect(kinds).toContain('search');
    expect(kinds).toContain('describe-tool');
    expect(kinds).toContain('configure');
    expect(kinds).toContain('execute-unconnected');
    expect(kinds).toContain('execute-unknown');
  });

  it('strips mock mode from the child environment instead of merely not setting it', () => {
    // An operator with MOCK_UNREAL_CONNECTION exported would otherwise turn a live
    // run into a mocked one without any signal that it happened.
    const env = loadEnv({ MOCK_UNREAL_CONNECTION: 'true', MCP_METRICS_PORT: '9100' });
    expect(env.MOCK_UNREAL_CONNECTION).toBeUndefined();
    expect(env.MCP_METRICS_PORT).toBeUndefined();
  });

  it('points the bridge at owned ports, never the editor lane pair', () => {
    const env = loadEnv({});
    expect(env.MCP_AUTOMATION_WS_PORTS).toBe(OWNED_WS_PORTS);
    for (const foreign of ['3000', '8090', '8091']) {
      expect(String(env.MCP_AUTOMATION_WS_PORTS).split(',')).not.toContain(foreign);
    }
    expect(env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK).toBe('false');
    expect(env.MCP_AUTOMATION_HOST).toBe('127.0.0.1');
  });

  it('classifies an unconnected execute as failed-closed, and a success as unexpected', () => {
    const refusal = { result: { structuredContent: { errorCode: 'NOT_CONNECTED' } } };
    expect(classifyResponse(refusal, 'execute-unconnected')).toBe('FAILED_CLOSED');
    // If an execute SUCCEEDED with no bridge, something answered that should not
    // have; that must never read as a pass.
    const success = { result: { structuredContent: { success: true } } };
    expect(classifyResponse(success, 'execute-unconnected')).toBe('UNEXPECTED');
    expect(classifyResponse(null, 'search')).toBe('TIMEOUT');
  });

  it('cycles only read-only capabilities in the soak', () => {
    // A soak that toggled a destructive capability would be mutating the editor as
    // a side effect of a memory measurement.
    for (const tool of SOAK_TOOLS) {
      expect(tool).toMatch(/^manage_/u);
    }
    expect(new Set(SOAK_TOOLS).size).toBe(SOAK_TOOLS.length);
  });
});
