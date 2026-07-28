#!/usr/bin/env node
// Task 49 — THE live corpus runner. One corpus, two real drivers, one reader.
//
// This is deliberately the NINTH file in scripts/qa/ and the LAST one that should
// ever be written here. The eight before it (task40-security-matrix,
// task41-idempotency-probe, task42-stale-state-probe, task43-preview-compensation,
// task46-cross-transport-matrix, task-38-native-capture, task-38-parity-qa,
// task-37-stdio-qa) each re-implemented: HTTP framing, SSE parsing, stdio
// correlation, timeouts, oracle reads and cleanup. Every one of those is now in
// tests/unit/task-49/*.mjs, offline-tested, and shared. A new live scenario should
// be a DATA entry in live-corpus.mjs, not a new script.
//
// PRECONDITIONS (all of them, or the run is BLOCKED rather than scored):
//   - a live Unreal Editor, launched with -unattended. WITHOUT IT Map_Check blocks
//     the game thread on a UI prompt headless mode cannot present, the automation
//     queue never drains, and the suite truncates silently while reading as a pass:
//       UnrealEditor-Cmd <project> -nosplash -NullRHI -NoSound -unattended -stdout
//   - native /mcp reachable (default 127.0.0.1:3000)
//   - the WebSocket bridge reachable (127.0.0.1:8090,8091) for the stdio side
//   - MCP_QA_TOKEN (or MCP_AUTOMATION_CAPABILITY_TOKEN) exported when the plugin
//     has bRequireCapabilityToken=True. NEVER passed on the command line, never
//     written to the report.
//   - a FRESH dist/. The stdio driver refuses a stale build rather than rebuilding.
//
// MOCK MODE IS NOT LIVE EVIDENCE. MOCK_UNREAL_CONNECTION is refused outright here.
//
// Run: node scripts/qa/task49-live-corpus.mjs --out <file> [--transport both]

import { execFileSync } from 'node:child_process';

import { buildCorpus } from '../../tests/unit/task-49/live-corpus.mjs';
import { runScenario } from '../../tests/unit/task-49/live-corpus-runner.mjs';
import { NativeDriver } from '../../tests/unit/task-49/live-driver-native.mjs';
import { StdioDriver } from '../../tests/unit/task-49/live-driver-stdio.mjs';
import {
  ResourceLedger,
  readCapabilityToken,
  writeRedactedEvidence,
} from '../../tests/unit/task-49/live-resource-ledger.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const OUT = argOf('--out', '.omo/evidence/task-49/live-corpus-run.json');
const TRANSPORT = argOf('--transport', 'both');
const ONLY = argOf('--only', null);

/** How many of our own `node dist/cli.js` children are alive right now. */
function ownedChildCount(pid) {
  if (pid === null) return 0;
  try {
    execFileSync('kill', ['-0', String(pid)], { stdio: 'ignore' });
    return 1;
  } catch {
    return 0;
  }
}

async function runOnDriver(driver, scenarios, report) {
  const rows = [];
  for (const scenario of scenarios) {
    if (!scenario.requires.clients.includes(driver.kind)) {
      rows.push({ namespace: scenario.namespace, driver: driver.name, status: 'SKIPPED', detail: `not declared for ${driver.kind}` });
      process.stderr.write(`SKIPPED  ${driver.kind.padEnd(7)} ${scenario.namespace}\n`);
      continue;
    }
    if (ONLY !== null && !scenario.namespace.includes(ONLY)) continue;
    const row = await runScenario(driver, scenario);
    rows.push(row);
    process.stderr.write(`${String(row.status).padEnd(8)} ${driver.kind.padEnd(7)} ${scenario.namespace}\n`);
  }
  report.rows.push(...rows);
  return rows;
}

async function main() {
  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    process.stderr.write('REFUSING TO RUN: MOCK_UNREAL_CONNECTION=true.\n'
      + '  A mocked run is not live evidence and must never be recorded as one.\n'
      + '  Unset it and point this probe at a real editor.\n');
    process.exitCode = 2;
    return;
  }

  const credentials = readCapabilityToken();
  const scenarios = buildCorpus();
  const ledger = new ResourceLedger();
  const report = {
    probe: 'task49-live-corpus',
    startedAt: new Date().toISOString(),
    transport: TRANSPORT,
    scenarioCount: scenarios.length,
    // The SOURCE of the token, never the token. A reader must be able to tell an
    // authenticated run from an unauthenticated one without the secret leaking.
    capabilityTokenSource: credentials.source,
    corpus: scenarios.map((scenario) => ({
      namespace: scenario.namespace,
      primitive: scenario.primitive,
      form: scenario.form,
      capability: scenario.capability,
      expected: scenario.expected.text,
      expectedErrorCode: scenario.expectedErrorCode,
      timeoutTier: scenario.timeoutTier,
      mutates: scenario.mutates,
      oracle: scenario.oracle === null ? null : { capability: scenario.oracle.capability, expect: scenario.oracle.expect },
      cleanupSteps: scenario.cleanup.length,
      clients: scenario.requires.clients,
      ownedPath: scenario.ownedPath,
    })),
    transports: {},
    rows: [],
    blocked: [],
  };

  try {
    if (TRANSPORT === 'both' || TRANSPORT === 'native') {
      const native = new NativeDriver();
      native.kind = 'native';
      const opened = await native.initialize();
      report.transports.native = {
        ready: opened.ok,
        status: opened.status,
        requestedVersion: opened.requestedVersion,
        negotiatedVersion: opened.negotiatedVersion,
        tokenPresented: opened.tokenPresented,
      };
      if (opened.ok) {
        ledger.register('session', String(opened.sessionId), { transport: 'native' },
          () => native.close(), () => native.verifySessionReleased());
        const stream = await native.openNotificationStream();
        report.transports.native.notificationStream = stream.reason;
        await runOnDriver(native, scenarios, report);
      } else {
        report.blocked.push('native /mcp did not initialize; a run with one side down is BLOCKED, never a pass');
      }
    }

    if (TRANSPORT === 'both' || TRANSPORT === 'stdio') {
      const stdio = new StdioDriver();
      stdio.kind = 'stdio';
      const started = await stdio.start();
      report.transports.stdio = {
        ready: started.ok,
        reason: started.reason,
        pid: started.pid,
        buildUnderTest: stdio.buildUnderTest,
        negotiatedVersion: started.negotiatedVersion,
        tokenPresented: started.tokenPresented,
      };
      if (started.ok) {
        const pid = started.pid;
        ledger.register('process', `node-dist-cli-${pid}`, { pid, entry: stdio.buildUnderTest?.entry },
          () => stdio.close(), async () => {
            const verdict = stdio.verifyChildReleased();
            const alive = ownedChildCount(pid);
            return {
              released: verdict.released && alive === 0,
              observed: `${verdict.observed}; kill -0 finds ${alive} live process(es) for pid ${pid}`,
            };
          });
        const bridge = await stdio.waitForBridge();
        report.transports.stdio.bridgeReady = bridge.ready;
        report.transports.stdio.bridgeAttempts = bridge.attempts;
        if (bridge.ready) {
          await runOnDriver(stdio, scenarios, report);
        } else {
          report.blocked.push('the WebSocket bridge never connected; every stdio case would answer NOT_CONNECTED');
        }
      } else {
        report.blocked.push(`stdio server did not start (${started.reason})`);
      }
    }
  } finally {
    report.teardown = await ledger.teardown();
    report.finishedAt = new Date().toISOString();
    report.summary = report.rows.reduce((totals, row) => {
      totals[row.status] = (totals[row.status] ?? 0) + 1;
      return totals;
    }, {});
    // The two facts a reader must be able to check without re-deriving anything.
    report.startedEqualsCompleted = report.rows.length === (report.summary.PASS ?? 0)
      + (report.summary.FAIL ?? 0) + (report.summary.BLOCKED ?? 0) + (report.summary.SKIPPED ?? 0);
    // Content leakage is counted SEPARATELY from process/session leakage. Run 1
    // reported `cleanupClean: true` with two materials still on disk, because the
    // ledger only knew about the child process and the session. A cleanup claim
    // that cannot see the thing it failed to clean up is worse than no claim.
    const contentLeaks = report.rows.filter((row) => row.cleanupVerified === false)
      .map((row) => ({ namespace: row.namespace, driver: row.driver, reading: row.cleanupReading }));
    report.contentLeaks = contentLeaks;
    report.cleanupClean = report.teardown.leaked === 0 && contentLeaks.length === 0;
    const written = writeRedactedEvidence(OUT, report);
    process.stderr.write(`\n${JSON.stringify(report.summary)}\n`);
    process.stderr.write(`teardown: ${report.teardown.released}/${report.teardown.total} released, ${report.teardown.leaked} leaked\n`);
    process.stderr.write(`content:  ${contentLeaks.length} scenario(s) left an asset behind\n`);
    if (report.blocked.length > 0) process.stderr.write(`BLOCKED: ${report.blocked.join(' | ')}\n`);
    process.stderr.write(`wrote ${written}\n`);
  }
}

main().catch((error) => {
  // A staleness refusal or a corpus rejection is an OPERATOR INSTRUCTION, not a
  // crash; a stack trace buries the one line that says what to do about it.
  const named = error?.name === 'StaleBuildRefusal' || error?.name === 'CorpusRejection';
  process.stderr.write(`${named ? error.message : String(error?.stack ?? error)}\n`);
  process.exitCode = 1;
});
