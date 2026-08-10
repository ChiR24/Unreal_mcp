// @ts-check
// tests/unit/engine-certification/certification-drivers.mjs
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

import { buildCorpus } from '../live-drivers/live-corpus.mjs';
import { runScenario } from '../live-drivers/live-corpus-runner.mjs';
import { NativeDriver } from '../live-drivers/live-driver-native.mjs';
import { StdioDriver } from '../live-drivers/live-driver-stdio.mjs';
import { observeProcess } from '../evidence-oracles/state-oracles.mjs';

/**
 * The per-engine subset. Every gateway primitive, both execute input forms, and
 * both polarities of the same oracle so neither reading can be explained away.
 */
/**
 * Scenarios excluded from stdio because their cleanup deletes something.
 *
 * EMPTY, and it must stay empty unless a LIVE measurement refills it. The entry
 * that used to be here excluded every destructive scenario from stdio on the
 * strength of a diagnosis that live QA has since disproved (see F2_SUPERSEDED
 * below). Its real effect was that a stdio-driven fixture created an asset and
 * never deleted it, so each certification left .uasset bytes on disk while
 * reporting green on a set chosen to exclude the failing case.
 */
export const DESTRUCTIVE_CLEANUP_NATIVE_ONLY = Object.freeze(/** @type {readonly string[]} */ ([]));

/**
 * The corrected record of what is actually true across transports here, kept as
 * a superseding entry rather than a deletion so the earlier claim stays legible.
 *
 * Measured live on UE 5.7.4, both transports, with the same asset fixture:
 *   canonical `asset.delete` + consent naming `asset.delete`
 *     -> stdio SUCCESS (file removed from disk), native SUCCESS
 *   alias `asset.delete_asset` + consent naming `asset.delete_asset`
 *     -> stdio refused, native SUCCESS (file removed)
 *
 * So the capability IS reachable over stdio and destructive scenarios can and do
 * clean up after themselves. What genuinely diverges is narrower: which NAME a
 * consent grant must use for the ALIAS request form. The plugin's CheckConsent
 * (Foundation/McpCapabilityAuthorization.cpp) demands the grant name the exact
 * capability id it resolved; over stdio the alias is canonicalised to
 * `asset.delete` before that check while the TypeScript layer
 * (gateway-execute-policy.ts) validated the grant against the alias record id,
 * so the alias form alone is caught between two different required names. Over
 * native the alias is not canonicalised before the same check, so the alias
 * grant matches. The canonical form is unambiguous on both.
 */
export const KNOWN_TRANSPORT_DEFECTS = Object.freeze([{
  id: 'F2_SUPERSEDED',
  supersedes: 'F2',
  capability: 'asset.delete_asset',
  transport: 'stdio',
  supersededClaim: 'asset.delete_asset is unreachable over stdio, so no client-supplied grant '
    + 'satisfies both layers and destructive cleanup must be native-only',
  summary: 'the ALIAS request form asset.delete_asset is the only unsatisfiable shape over stdio: '
    + 'TypeScript validates the grant against the alias record id while the plugin canonicalises '
    + 'to asset.delete before its own consent check, so the two layers demand different names. '
    + 'The CANONICAL form asset.delete is accepted by both layers and deletes over stdio.',
  effect: 'a stdio-driven destructive fixture cleans up correctly when its consent grant names the '
    + 'canonical capability; the earlier blanket exclusion was broader than the evidence supported '
    + 'and left real .uasset bytes on disk',
  source: 'live UE 5.7.4 two-transport measurement; supersedes Task 49 finding F2',
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
  // Added once destructive scenarios stopped being excluded from stdio: this is
  // the ONLY case that drives the legacy execute form through a real mutation
  // AND its delete, on both transports. While it sat outside the subset the
  // certification could report green without ever deleting anything over stdio.
  'task49.execute.legacy-mutation',
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
  let started;
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
            detail: 'excluded from this transport by DESTRUCTIVE_CLEANUP_NATIVE_ONLY',
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
