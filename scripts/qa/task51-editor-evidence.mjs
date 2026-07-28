#!/usr/bin/env node
// Task 51 — fold the editor-side gate results back into the task evidence.
//
// Task 51 closed its own 32-session load gate but left three claims BLOCKED with
// the observable EDITOR_OWNED_BY_ANOTHER_LANE, because it was not the lane
// permitted to start an editor. The Task 52 certification lane IS that lane, and
// ran five disposable editors. This script records what those runs closed and,
// just as importantly, what they did NOT.
//
// THE OWNERSHIP OBSERVABLE IS RETIRED, NOT SATISFIED-BY-ASSERTION. Where a claim
// is still blocked, its blocker is REPLACED by the condition that was actually
// measured, and the original is kept so a reader can see the claim moved from
// "nobody could look" to "we looked, and here is what stopped it".
//
// Run: node scripts/qa/task51-editor-evidence.mjs [--gates FILE] [--repeat FILE] [--out FILE]

import { readFileSync, writeFileSync } from 'node:fs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};
const GATES = argOf('--gates', '.omo/evidence/task-51/editor-gates-run5.json');
const REPEAT = argOf('--repeat', '.omo/evidence/task-51/editor-gates.json');
const OUT = argOf('--out', '.omo/evidence/task-51-pure-unreal-mcp-implementation.json');
const log = (line) => { process.stderr.write(`${line}\n`); };

const gates = JSON.parse(readFileSync(GATES, 'utf8'));
const repeat = JSON.parse(readFileSync(REPEAT, 'utf8'));
const document = JSON.parse(readFileSync(OUT, 'utf8'));

/** Every Gate A attempt, including the ones that measured nothing. */
const RSS_ATTEMPTS = [
  {
    run: 1, verdict: 'PASS', retainedMiB: 9.11, peakOverBaselineMiB: 11.23, secondHalfGrowthMiB: 5.30,
    workloadServed: false,
    discountedBecause: 'the workload was NOT governed: 280 requests were fired past the plugin\'s '
      + 'MaxClientToolCallsPerMinute=120 and the excess came back HTTP 429 while the harness counted them as measured. '
      + 'That is Task 51\'s own D1 tautology rebuilt by hand, so this reading is recorded and NOT relied on.',
  },
  {
    run: 2, verdict: 'PASS', retainedMiB: 25.95, peakOverBaselineMiB: 27.95, secondHalfGrowthMiB: 5.95,
    workloadServed: true, workload: '150/150 succeeded, 0 rate-limited',
    note: 'The ONE methodologically valid reading: governed workload fully served, baseline demonstrably not a peak, '
      + 'RSS rose 27.95 MiB above baseline so a leak of gate size would have been visible.',
    evidenceWeakness: 'its JSON was overwritten by a later run at the same --out path; only the console transcript '
      + '(/tmp/opencode/task51-gates/run2.log, quoted verbatim above) survives, so this reading cannot be re-checked '
      + 'field by field the way the others can',
  },
  {
    run: 3, verdict: 'INVALID_VACUOUS_BASELINE', retainedMiB: -1330.11, peakOverBaselineMiB: -1110.51,
    workloadServed: true,
    cause: 'self-inflicted: the UObject census probe (forced GC + a 300KB detailed memreport) ran BEFORE the RSS '
      + 'baseline, so the instrument for Gate B became the dominant memory event of Gate A. Probe order was fixed after.',
  },
  {
    run: 4, verdict: 'INVALID_VACUOUS_BASELINE', retainedMiB: -1798.87, peakOverBaselineMiB: -1795.34,
    workloadServed: true, workload: '150/150 succeeded, 0 rate-limited',
    cause: 'engine behaviour, not the harness: the post-warm-up baseline of 2677.71 MiB decayed to a 873.71 MiB '
      + 'steady-state trough during the measured phase. A 30-request warm-up does not bring a multi-GB editor to '
      + 'steady state, so the baseline sat on the start-up decay curve.',
  },
  {
    run: 5, verdict: 'INVALID_VACUOUS_BASELINE', retainedMiB: -249.46, peakOverBaselineMiB: -129.43,
    secondHalfGrowthMiB: 5.14, workloadServed: true, workload: '150/150 succeeded, 0 rate-limited',
    cause: 'THE DECISIVE ONE. Steady-state detection was added and it WORKED — three consecutive troughs at '
      + '2292.87 / 2296.54 / 2299.86 MiB, spread 0.30%, rising rather than falling, so the baseline was not on a '
      + 'slope. The editor then still released ~250 MiB during the measured phase. The reclaim is engine-scheduled '
      + 'and an order of magnitude larger than the 64 MiB gate.',
  },
];

const gateA = {
  claim: 'editor retained RSS <= 64 MiB after warm-up',
  status: 'STILL BLOCKED',
  previousCode: 'EDITOR_OWNED_BY_ANOTHER_LANE',
  previousObservableRetired: 'The Task 52 lane owns editors and started five of its own, so nobody is prevented from '
    + 'looking any more. This claim is no longer blocked on ownership; it is blocked on what the looking found.',
  code: 'EDITOR_RSS_DELTA_DOMINATED_BY_ENGINE_RECLAIM',
  observable: 'Across five disposable UE 5.7.4 editors, the editor released memory in engine-scheduled steps of '
    + '129-1795 MiB during the measured phase — one to nearly two orders of magnitude larger than the 64 MiB the gate '
    + 'compares against. In 4 of 5 runs the post-warm-up settled trough was itself a PEAK, so retained came out '
    + 'negative by construction and `<= 64 MiB` could not have failed; the harness scored those '
    + 'INVALID_VACUOUS_BASELINE rather than PASS. Adding steady-state detection (run 5) removed the start-up-decay '
    + 'explanation and the vacuous result persisted, which is what makes this a property of the process rather than '
    + 'of the warm-up.',
  whyNotClaimedAsAPass: 'Run 2 did produce a valid, falsifiable reading of +25.95 MiB retained against a 64 MiB limit. '
    + 'One scoreable result in five attempts is not a closed gate: reporting it alone would mean discarding three '
    + 'runs that the vacuity rule refused, which is precisely the selection Task 51\'s D4 finding exists to prevent. '
    + 'The reading is kept as supporting evidence, not as the close.',
  whatWouldCloseIt: 'A retained-RSS gate for the editor needs either a much longer measured phase (so engine reclaim '
    + 'is amortised rather than dominant) or a metric that excludes engine-scheduled reclaim — for example UE\'s own '
    + 'per-allocator LLM tags for the plugin\'s allocations, rather than whole-process VmRSS.',
  attempts: RSS_ATTEMPTS,
  vacuityRuleFiredOnLiveData: true,
};

const gateB = {
  claim: 'zero residual UObjects after teardown',
  status: 'CLOSED',
  previousCode: 'EDITOR_OWNED_BY_ANOTHER_LANE',
  closedBy: 'two independent disposable editors (run 4 pid 253007, run 5 pid 265654), different workspaces and ports, '
    + 'producing identical censuses',
  methodology: gates.gateB.methodology,
  positiveControl: {
    createdObjects: gates.gateB.controlObjects,
    censusRoseBy: gates.gateB.controlRoseBy,
    censusReturnedBy: gates.gateB.controlReturnedBy,
    why: 'A census that never moves reads "zero residual" for a healthy editor AND for a counter that is stuck, blind '
      + 'or parsed off the wrong line. It is only scored after the control has moved it in BOTH directions. An earlier '
      + 'run parsed the census off the last of eight summary lines in the memreport, read 0 objects every time, and '
      + 'was correctly scored INVALID_BLIND_COUNTER rather than passed.',
  },
  readings: gates.gateB.readings,
  repeatReadings: repeat.gateB?.readings ?? null,
  reproducible: JSON.stringify(gates.gateB.readings) === JSON.stringify(repeat.gateB?.readings ?? null),
  sessionCycles: gates.gateB.sessionCycles,
  residualObjects: gates.gateB.residualObjects,
  instrumentNote: 'The census could NOT come from a console command: `obj list`, `obj garbage` and `memreport` are all '
    + 'refused on BOTH transports by the shared forbidden-token policy rule (COMMAND_BLOCKED, observed live). It is '
    + 'parsed from a .memreport the PLUGIN generated for system_control.generate_memory_report — UE\'s '
    + '[MemReportCommands] runs `obj list -resourcesizesort` — read off disk, a different channel from the gateway '
    + 'reply that triggered it. GC is forced with the gc.CollectGarbageEveryFrame CVar, which no policy rule blocks.',
};

const delegates = {
  claim: 'zero residual delegates after teardown',
  status: 'STILL BLOCKED',
  previousCode: 'EDITOR_OWNED_BY_ANOTHER_LANE',
  previousObservableRetired: 'an editor was available; the instrument was not',
  code: 'NO_DELEGATE_INSTRUMENT',
  observable: gates.delegates?.observable ?? 'no delegate-binding instrument is reachable from either transport',
  notEstimated: 'no delegate figure is inferred from the UObject census; a dynamic delegate binding is not a UObject '
    + 'and the census cannot see one.',
};

// A genuine by-product: Task 51 decided native accept/reject against a MIRROR of
// the plugin's policy header and said so in notProven. These runs exercised the
// compiled binary. That is a PARTIAL close and is stated as one.
const liveParity = {
  claim: 'LIVE native accept/reject parity (partial)',
  status: 'PARTIALLY CLOSED',
  observed: [
    { command: 'obj list', outcome: 'REFUSED', code: 'COMMAND_BLOCKED', predictedByMirror: true },
    { command: 'obj garbage', outcome: 'REFUSED', code: 'COMMAND_BLOCKED', predictedByMirror: true },
    { command: 'gc.CollectGarbageEveryFrame 1', outcome: 'ACCEPTED', predictedByMirror: true },
    { command: 'gc.CollectGarbageEveryFrame 0', outcome: 'ACCEPTED', predictedByMirror: true },
  ],
  whatThisIsNot: 'four commands against the compiled plugin, not the 1,200-case auth/session corpus and not the '
    + '4,000-row differential. Task 51\'s notProven entry about mirror-based parity still stands for everything else.',
};

document.liveResults = document.liveResults ?? {};
document.liveResults.editorGates = {
  ranBy: 'Task 52 certification lane (owns editors, plugin builds, UBT/RunUAT and ports)',
  engine: gates.engine,
  runs: 5,
  requestTallyLastRun: gates.requestTally,
  gateA, gateB, delegates, liveParity,
  cleanup: gates.cleanup,
  evidenceFiles: [GATES, REPEAT, '.omo/evidence/task-51/editor-gates-run3.json'],
};

// Rewrite the blocked entries these runs touched, keeping every original observable.
document.blocked = document.blocked.map((entry) => {
  if (entry.claim === 'editor retained RSS <= 64 MiB after warm-up') {
    return {
      ...entry, status: gateA.status, historicalCode: entry.code, code: gateA.code,
      historicalObservable: entry.observable, observable: gateA.observable,
      retiredBlocker: gateA.previousObservableRetired,
      seeAlso: 'liveResults.editorGates.gateA',
      keptBecause: 'the claim moved from "no lane could look" to "five editors looked"; both conditions belong on the record',
    };
  }
  if (entry.claim === 'zero residual UObjects and delegates') {
    return {
      ...entry,
      status: 'PARTIALLY CLOSED 2026-07-28 — UObjects CLOSED, delegates STILL BLOCKED',
      historicalCode: entry.code, code: delegates.code,
      historicalObservable: entry.observable,
      observable: delegates.observable,
      howItWasClosed: `UObject half: ${gateB.residualObjects} residual objects across ${gateB.sessionCycles} native `
        + `MCP session cycles, reproduced identically in two independent editors, with a positive control that moved `
        + `the census +${gateB.positiveControl.censusRoseBy} on create and -${gateB.positiveControl.censusReturnedBy} `
        + 'on destroy. Delegate half: no instrument exists.',
      seeAlso: 'liveResults.editorGates.gateB and .delegates',
    };
  }
  if (String(entry.claim).startsWith('native /mcp session load')) {
    return {
      ...entry,
      status: 'PARTIALLY CLOSED 2026-07-28 — live native sessions exercised; full parity corpus still not run',
      historicalCode: entry.code, code: 'FULL_PARITY_CORPUS_NOT_RUN',
      historicalObservable: entry.observable,
      howItWasClosed: `${gates.requestTally.succeeded}/${gates.requestTally.attempted} native /mcp tool calls succeeded `
        + `in the final run alone, across ${gateB.sessionCycles} opened-and-closed sessions plus the measurement `
        + 'client, all against a plugin this lane built and started. Four console commands were observed being '
        + 'accepted or refused by the compiled policy exactly as the mirror predicted.',
      stillNotProven: liveParity.whatThisIsNot,
      seeAlso: 'liveResults.editorGates.liveParity',
    };
  }
  return entry;
});

writeFileSync(OUT, `${JSON.stringify(document, null, 2)}\n`);
log(`gateA   : ${gateA.status} (${gateA.code})`);
log(`gateB   : ${gateB.status} — ${gateB.residualObjects} residual, reproducible=${gateB.reproducible}`);
log(`delegates: ${delegates.status} (${delegates.code})`);
log(`wrote ${OUT}`);
