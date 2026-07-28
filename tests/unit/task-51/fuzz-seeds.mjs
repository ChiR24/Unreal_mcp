// @ts-check
// tests/unit/task-51/fuzz-seeds.mjs
// Task 51 — the FIXED seeds and time budgets. This file is the reason a finding in
// this suite can be handed to someone else.
//
// SEEDS ARE PINNED, NOT ROLLED. A suite that seeds from the clock finds more bugs
// per week and can prove none of them: the run that failed is gone, the artifact
// says "seed 1753649201337", and nobody can tell whether today's green means fixed
// or means a different corpus. Pinned seeds make every run comparable and make
// "this regressed" a statement about the code rather than about the dice.
//
// GROWING COVERAGE IS DONE BY ADDING A SEED, NOT BY RANDOMISING ONE. Append a new
// named entry; the existing entries keep reproducing the corpus already reasoned
// about. `MCP_FUZZ_EXTRA_SEED` exists for an operator who wants to explore beyond
// the pinned set — it is additive, never a replacement, and a finding it produces
// must be pinned here before it counts as covered.
//
// BUDGETS ARE CASE COUNTS, NOT WALL CLOCK. A time-bounded fuzz loop generates a
// different number of cases on a loaded machine, so a green run proves nothing about
// what was executed. Counts make the executed corpus identical everywhere; the time
// ceilings below exist only to fail a run that has hung, not to size it.

/** Pinned seeds, one per concern. Changing a value invalidates recorded findings. */
export const SEEDS = Object.freeze({
  commands: 0x5111c0de,
  paths: 0x51110a74,
  protocol: 0x51119a0c,
  auth: 0x5111a047,
  ledger: 0x5111d6e2,
  load: 0x51110ad0,
  soak: 0x51115a0c,
});

/** Case counts. Deterministic across machines, unlike a time budget. */
export const BUDGETS = Object.freeze({
  commandCases: 4000,
  pathCases: 2000,
  protocolCases: 1500,
  authCases: 1200,
  ledgerOperations: 400,
  ledgerRuns: 25,
  /** Plan line 503: 32 isolated sessions and 1,000 mixed requests. */
  loadSessions: 32,
  loadRequests: 1000,
  /** Plan line 503: a 500-cycle cleanup soak. */
  soakCycles: 500,
});

/** Wall-clock ceilings. A run that exceeds one has HUNG; it is never a sizing knob. */
export const TIME_LIMITS_MS = Object.freeze({
  offlineProperty: 30_000,
  loadRun: 600_000,
  soakRun: 600_000,
});

/** Retained-RSS gates from plan line 503, in bytes. */
export const RSS_LIMITS = Object.freeze({
  nodeRetainedBytes: 32 * 1024 * 1024,
  editorRetainedBytes: 64 * 1024 * 1024,
});

/**
 * An optional additional seed supplied by an operator. Additive only.
 * @param {NodeJS.ProcessEnv} [env]
 * @returns {number|string|null}
 */
export function extraSeed(env = process.env) {
  const raw = env.MCP_FUZZ_EXTRA_SEED;
  if (typeof raw !== 'string' || raw.length === 0) return null;
  const numeric = Number(raw);
  return Number.isInteger(numeric) ? numeric : raw;
}
