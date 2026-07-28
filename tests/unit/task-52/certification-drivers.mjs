// @ts-check
// tests/unit/task-52/certification-drivers.mjs
// Task 52 — running BOTH live drivers against the disposable editor.
//
// The drivers, the corpus and the scenario runner are Task 49's. Re-implementing
// them here would give this lane its own subtly different idea of what "the
// gateway answered correctly" means, which is exactly the divergence that made
// the eight one-off probes in scripts/qa/ mutually incomparable. This file only
// does the two things Task 49's runner cannot know about:
//
//   it points both drivers at THIS run's allocated ports rather than the
//   wave-wide 3000/8090/8091, so a certification can never be scored against an
//   editor it did not launch — the failure that looks most like success;
//
//   it takes a SUBSET of the corpus, because a certification is a per-engine
//   smoke of the whole surface and not a rerun of the full suite on every minor.
//   The subset is chosen so that it gives up none of the coverage DIMENSIONS the
//   corpus declares — every gateway primitive, both execute input forms, both
//   oracle polarities and all three protocol kinds — and drops only duplicates
//   of a dimension already covered. A subset that quietly lost a dimension would
//   still look like a full run in the report, which is the failure this costs
//   one assertion to prevent (see certification-drivers.test.ts).
//
// The mutation case is kept deliberately. Its assets land inside the disposable
// project, so the run proves the create -> independent-read -> delete ->
// re-read loop end to end on this engine and still leaves nothing behind.

import { buildCorpus } from '../task-49/live-corpus.mjs';
import { runScenario } from '../task-49/live-corpus-runner.mjs';
import { NativeDriver } from '../task-49/live-driver-native.mjs';
import { StdioDriver } from '../task-49/live-driver-stdio.mjs';
import { observeProcess } from '../task-50/state-oracles.mjs';

/**
 * The per-engine subset. Every gateway primitive, both execute input forms, and
 * both polarities of the same oracle so neither reading can be explained away.
 */
/**
 * Scenarios whose cleanup DELETES something, which is native-only.
 *
 * Task 49 finding F2: `asset.delete_asset` is unreachable over stdio. The
 * TypeScript gateway validates the consent grant against the alias record while
 * the plugin normalises to canonical `asset.delete`, so no client-supplied grant
 * satisfies both layers and every stdio delete is refused. Task 50 reached the
 * same conclusion and routes destructive work over native for this reason.
 *
 * Running the mutation on stdio anyway does not test the product harder — it
 * re-fails one known, already-filed cross-transport defect on every engine
 * certified, which drowns the per-engine signal this run exists to produce. The
 * defect is recorded in the report instead of being re-discovered nine times.
 */
export const DESTRUCTIVE_CLEANUP_NATIVE_ONLY = Object.freeze(['task49.execute.canonical-mutation']);

/** The known cross-transport defect the line above encodes, carried into evidence. */
export const KNOWN_TRANSPORT_DEFECTS = Object.freeze([{
  id: 'F2',
  capability: 'asset.delete_asset',
  transport: 'stdio',
  summary: 'the TypeScript gateway checks consent against the alias record while the plugin '
    + 'normalises to canonical asset.delete, so no client-supplied grant satisfies both layers',
  effect: 'a stdio-driven destructive fixture cannot clean up after itself',
  source: 'Task 49 finding F2; Task 50 routes destructive cleanup over native for the same reason',
}]);

export const CERTIFICATION_SUBSET = Object.freeze([
  'task49.search.keyword',
  'task49.describe.tool-summary',
  'task49.describe.action-params',
  // The guided-error path is per-engine behavior too: a describe that stops
  // suggesting a next call on one minor is a regression a happy-path-only subset
  // would carry silently.
  'task49.describe.unknown-tool',
  'task49.configure.get-status',
  'task49.execute.canonical-read',
  'task49.execute.legacy-read',
  'task49.execute.canonical-mutation',
  'task49.execute.consent-refused',
  'task49.progress.token-not-invented',
  'task49.task.search-checkpoint',
  'task49.cancel.does-not-wedge',
]);

/**
 * What the subset deliberately leaves to the full suite, and why. Stated here so
 * the omission is a decision on the record rather than a gap someone has to
 * reconstruct from the diff.
 */
export const SUBSET_OMISSIONS = Object.freeze([
  { namespace: 'task49.execute.legacy-mutation', reason: 'a second real mutation with the same semantics as the canonical one; the legacy FORM is already exercised by task49.execute.legacy-read, so this only doubles the per-engine fixture cost' },
  { namespace: 'task49.task.execute-refused', reason: 'declared for stdio only, and the task protocol kind is already covered by task49.task.search-checkpoint on both transports' },
]);

/**
 * @param {{ ports: Record<string, number>, repoRoot: string, aggregator: any, projectDir: string }} spec
 */
export async function runCertificationDrivers(spec) {
  const scenarios = buildCorpus().filter((scenario) => CERTIFICATION_SUBSET.includes(scenario.namespace));
  /** @type {Array<Record<string, unknown>>} */
  const rows = [];
  /** @type {any} */
  const report = {
    native: { ok: false, detail: 'not attempted' },
    stdio: { ok: false, detail: 'not attempted' },
    corpus: { total: 0, pass: 0, fail: 0, blocked: 0, skipped: 0, transports: /** @type {string[]} */ ([]), rows },
    subset: [...CERTIFICATION_SUBSET],
    knownTransportDefects: KNOWN_TRANSPORT_DEFECTS.map((entry) => ({ ...entry })),
    teardown: /** @type {Array<Record<string, unknown>>} */ ([]),
  };

  const native = /** @type {any} */ (new NativeDriver({ port: spec.ports.native, clientName: 'task52-certification' }));
  native.kind = 'native';
  const opened = await native.initialize();
  report.native = {
    ok: opened.ok === true,
    detail: opened.ok === true
      ? `initialized on 127.0.0.1:${spec.ports.native}, protocol ${String(opened.negotiatedVersion)}`
      : `native /mcp did not initialize (status ${String(opened.status)})`,
    port: spec.ports.native,
    negotiatedVersion: opened.negotiatedVersion ?? null,
    sessionId: opened.ok === true ? String(opened.sessionId) : null,
  };

  // The stdio server reaches the editor through the WebSocket bridge, so it is
  // steered at THIS run's bridge port. Without that it would find the wave-wide
  // 8090/8091 — or nothing — and answer NOT_CONNECTED for every case, which reads
  // identically to a broken plugin.
  const stdio = /** @type {any} */ (new StdioDriver({
    cwd: spec.repoRoot,
    clientName: 'task52-certification',
    env: {
      ...process.env,
      MCP_AUTOMATION_HOST: '127.0.0.1',
      MCP_AUTOMATION_PORT: String(spec.ports.wsPrimary),
      MCP_LOG_LEVEL: 'error',
    },
  }));
  stdio.kind = 'stdio';
  /** @type {any} */
  let started = { ok: false, reason: 'NOT_STARTED', pid: null };
  try {
    started = await stdio.start();
  } catch (error) {
    started = { ok: false, reason: error instanceof Error ? error.message : String(error), pid: null };
  }
  /** @type {any} */
  let bridge = { ready: false, attempts: 0 };
  if (started.ok) bridge = await stdio.waitForBridge();
  report.stdio = {
    ok: started.ok === true && bridge.ready === true,
    detail: started.ok !== true
      ? `stdio server did not start (${String(started.reason)})`
      : bridge.ready
        ? `bridge connected to 127.0.0.1:${spec.ports.wsPrimary} after ${bridge.attempts} attempts`
        : `the WebSocket bridge never reached 127.0.0.1:${spec.ports.wsPrimary}; every stdio case would answer NOT_CONNECTED`,
    port: spec.ports.wsPrimary,
    pid: started.pid ?? null,
    buildUnderTest: stdio.buildUnderTest ?? null,
    negotiatedVersion: started.negotiatedVersion ?? null,
  };
  if (started.ok && started.pid !== null) {
    spec.aggregator.recordProcess({ pid: started.pid, role: 'node dist/cli.js (spawned and owned by this certification)' });
  }

  const drivers = [
    ...(report.native.ok ? [native] : []),
    ...(report.stdio.ok ? [stdio] : []),
  ];
  report.corpus.transports = drivers.map((driver) => driver.kind);

  try {
    for (const driver of drivers) {
      for (const scenario of scenarios) {
        if (!scenario.requires.clients.includes(driver.kind)) {
          rows.push({ namespace: scenario.namespace, driver: driver.kind, status: 'SKIPPED', detail: `not declared for ${driver.kind}` });
          continue;
        }
        if (driver.kind !== 'native' && DESTRUCTIVE_CLEANUP_NATIVE_ONLY.includes(scenario.namespace)) {
          rows.push({
            namespace: scenario.namespace, driver: driver.kind, status: 'SKIPPED',
            detail: 'destructive cleanup is native-only (Task 49 finding F2); running it here would leave a fixture behind',
          });
          continue;
        }
        const row = await runScenario(driver, scenario);
        rows.push({
          namespace: row.namespace, driver: driver.kind, status: row.status,
          expected: scenario.expected.text, observed: row.judgement?.reason ?? null,
          errorCode: row.errorCode ?? null,
          oracle: row.oracle === undefined ? null : { pass: row.oracle.pass, reading: row.oracle.reading ?? null },
          cleanupVerified: row.cleanupVerified ?? null,
          ms: row.ms ?? null,
        });
      }
    }
  } finally {
    /** @type {Array<{ name: string, driver: any, pid: number|null }>} */
    const closable = [
      { name: 'native', driver: native, pid: null },
      { name: 'stdio', driver: stdio, pid: started.pid ?? null },
    ];
    for (const { name, driver, pid } of closable) {
      try {
        await driver.close();
      } catch { /* a driver that never opened cannot fail to close */ }
      const released = pid === null ? true : observeProcess({ pid }).present !== true;
      report.teardown.push({
        driver: name, released,
        observed: pid === null ? 'session closed; release proven by the orchestrator port check' : `/proc/${pid} present=${String(!released)}`,
      });
    }
  }

  report.corpus.total = rows.length;
  for (const row of rows) {
    if (row.status === 'PASS') report.corpus.pass += 1;
    else if (row.status === 'SKIPPED') report.corpus.skipped += 1;
    else if (row.status === 'BLOCKED') report.corpus.blocked += 1;
    else report.corpus.fail += 1;
  }
  return report;
}
