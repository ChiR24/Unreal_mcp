// @ts-check
// tests/unit/task-50/evidence-aggregator.mjs
// Task 50 — assembling the evidence document the validator will then try to break.
//
// The aggregator and the validator are deliberately SEPARATE and deliberately do
// not share their checks. The aggregator records what happened; the validator
// re-derives it from the filesystem and /proc and refuses if the two disagree.
// If one module both wrote and approved the document, a bug in the recording
// would be invisible — which is the same "compared the parser to itself" mistake
// that made Task 49's SSE tests vacuous.
//
// Everything is APPEND-ONLY and id-addressed. A claim points at observation ids
// and a cleanup id; nothing is embedded twice, so a document cannot say two
// different things about the same reading.

import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';

import { observeProcess } from './state-oracles.mjs';
import { auditPositiveControls } from './oracle-judgement.mjs';
import { snapshotArtifact, snapshotTree } from './evidence-validator.mjs';
import { writeRedactedEvidence } from '../task-49/live-resource-ledger.mjs';

/**
 * Run a command and record it verbatim with its exit code.
 *
 * The exit code is RECORDED, never interpreted as the verdict: a fully green
 * Unreal automation run returns non-zero because a handled engine ensure fires
 * before the first test. Judging by exit code would mark that run failed and
 * judging by "it printed something" would mark a truncated run passed.
 * @param {{ file: string, args?: readonly string[], cwd?: string, env?: NodeJS.ProcessEnv,
 *   timeoutMs?: number, maxOutput?: number }} spec
 */
export function recordCommand(spec) {
  const started = Date.now();
  /** @type {Record<string, unknown>} */
  const record = { cmd: [spec.file, ...(spec.args ?? [])].join(' '), cwd: spec.cwd ?? process.cwd(), startedAt: new Date(started).toISOString() };
  try {
    const stdout = execFileSync(spec.file, [...(spec.args ?? [])], {
      cwd: spec.cwd, env: spec.env, timeout: spec.timeoutMs ?? 300_000, encoding: 'utf8',
      maxBuffer: 32 * 1024 * 1024,
    });
    record.exitCode = 0;
    record.stdoutTail = String(stdout).slice(-(spec.maxOutput ?? 4000));
  } catch (error) {
    const failure = /** @type {{ status?: number|null, stdout?: string, stderr?: string, message?: string }} */ (error);
    record.exitCode = failure.status ?? null;
    record.stdoutTail = String(failure.stdout ?? '').slice(-(spec.maxOutput ?? 4000));
    record.stderrTail = String(failure.stderr ?? failure.message ?? '').slice(-(spec.maxOutput ?? 4000));
  }
  record.ms = Date.now() - started;
  return record;
}

/**
 * Identify the engine actually in use — from `Engine/Build/Build.version`, the
 * file the engine itself writes, never from the folder name. `/data/UnrealEngine`
 * holds 5.7.4; a probe that inferred "5.7" from a path would be right by luck and
 * silently wrong the first time a root is renamed.
 * @param {{ engineRoot: string, projectPath: string }} spec
 */
export function identifyEngine(spec) {
  const buildVersion = `${spec.engineRoot}/Engine/Build/Build.version`;
  /** @type {Record<string, unknown>} */
  const record = { engineRoot: spec.engineRoot, projectPath: spec.projectPath, buildVersionFile: buildVersion };
  const snapshot = snapshotArtifact({ projectRoot: '/', path: buildVersion });
  record.buildVersionSha256 = snapshot.sha256;
  try {
    const parsed = JSON.parse(readFileSync(buildVersion, 'utf8'));
    record.version = `${parsed.MajorVersion}.${parsed.MinorVersion}.${parsed.PatchVersion}`;
    record.branch = parsed.BranchName ?? null;
    record.changelist = parsed.Changelist ?? null;
  } catch (error) {
    record.version = null;
    record.readError = String(error instanceof Error ? error.message : error);
  }
  return record;
}

/**
 * The evidence document under construction.
 */
export class EvidenceAggregator {
  /**
   * @param {{ task: number, title: string, plan: string, kind?: string,
   *   projectRoot?: string, now?: () => Date }} spec
   */
  constructor(spec) {
    this.projectRoot = spec.projectRoot ?? process.cwd();
    this.now = spec.now ?? (() => new Date());
    this.document = /** @type {Record<string, any>} */ ({
      task: spec.task,
      title: spec.title,
      plan: spec.plan,
      kind: spec.kind ?? 'wave-6 oracle/evidence lane',
      generatedAt: this.now().toISOString(),
      environment: { mockUnrealConnection: process.env.MOCK_UNREAL_CONNECTION === 'true', processes: [] },
      tree: { files: [], sourceDigest: null },
      artifacts: [],
      engine: {},
      clients: [],
      commands: [],
      transcripts: [],
      observations: [],
      claims: [],
      cleanup: [],
      positiveControls: { ok: false, mechanisms: [], missing: ['no observation was recorded'] },
      notProven: [],
      notes: [],
    });
    this.sequence = 0;
  }

  /** @param {readonly string[]} files */
  recordTree(files) {
    this.document.tree = snapshotTree({ projectRoot: this.projectRoot, files });
    return this;
  }

  /** @param {{ path: string, inputsNewest?: string|null, inputsNewestAtMs?: number|null }} spec */
  recordArtifact(spec) {
    this.document.artifacts.push(snapshotArtifact({ projectRoot: this.projectRoot, ...spec }));
    return this;
  }

  /**
   * Record a process this run drove or spawned, WITH its start ticks.
   * @param {{ pid: number, role: string, procRoot?: string }} spec
   */
  recordProcess(spec) {
    const live = observeProcess({ pid: spec.pid, procRoot: spec.procRoot });
    this.document.environment.processes.push({
      pid: spec.pid,
      role: spec.role,
      startTicks: typeof live.detail.startTicks === 'number' ? live.detail.startTicks : null,
      comm: live.detail.comm ?? null,
      // Bounded and argv-only: a full cmdline can carry a token on the command
      // line of a process we did not spawn.
      cmdlinePreview: Array.isArray(live.detail.cmdline) ? live.detail.cmdline.slice(0, 3).join(' ').slice(0, 200) : null,
      aliveAtCapture: live.present === true,
      observedAt: live.observedAt,
    });
    return this;
  }

  /** @param {Record<string, unknown>} spec */
  recordClient(spec) {
    this.document.clients.push(spec);
    return this;
  }

  /** @param {Record<string, unknown>} command */
  addCommand(command) {
    this.document.commands.push(command);
    return this;
  }

  /**
   * A transcript entry: what went on the wire and what came back, bounded.
   * @param {{ id?: string, transport: string, request: unknown, response: unknown,
   *   ms?: number|null, note?: string }} spec
   */
  addTranscript(spec) {
    const id = spec.id ?? `tx-${++this.sequence}`;
    const bound = (/** @type {unknown} */ value) => JSON.stringify(value ?? null).slice(0, 4000);
    this.document.transcripts.push({
      id, transport: spec.transport, request: bound(spec.request), response: bound(spec.response),
      ms: spec.ms ?? null, note: spec.note ?? null, at: this.now().toISOString(),
    });
    return id;
  }

  /**
   * Store an observation and return its id, so a claim can cite it rather than
   * restate it.
   * @param {import('./state-oracles.mjs').Observation} entry
   * @param {{ id?: string, phase?: 'pre'|'post'|'cleanup'|'control' }} [meta]
   */
  addObservation(entry, meta = {}) {
    const id = meta.id ?? `obs-${++this.sequence}`;
    this.document.observations.push({ id, phase: meta.phase ?? 'post', ...entry });
    return id;
  }

  /**
   * @param {{ id?: string, target: string, effect: string, outcome: string,
   *   verdict: string, pass: boolean, reason: string, oracleRefs: readonly string[],
   *   cleanupRef?: string|null, transcriptRef?: string|null }} spec
   */
  addClaim(spec) {
    const id = spec.id ?? `claim-${++this.sequence}`;
    this.document.claims.push({ ...spec, id, oracleRefs: [...spec.oracleRefs], cleanupRef: spec.cleanupRef ?? null });
    return id;
  }

  /**
   * @param {{ id?: string, owned: string, verifiedBy: string, pass: boolean,
   *   verdict: string, reason: string, receipts?: unknown }} spec
   */
  addCleanup(spec) {
    const id = spec.id ?? `cleanup-${++this.sequence}`;
    this.document.cleanup.push({ ...spec, id });
    return id;
  }

  /** @param {string} entry */
  addNotProven(entry) {
    this.document.notProven.push(entry);
    return this;
  }

  /** @param {string} entry */
  addNote(entry) {
    this.document.notes.push(entry);
    return this;
  }

  /**
   * Derive the positive-control audit from the observations actually recorded,
   * rather than letting a caller assert it. A claim of "we had controls" that the
   * document's own readings do not support is the overclaim this section exists
   * to prevent.
   * @param {string} verdict
   */
  finalize(verdict) {
    this.document.positiveControls = auditPositiveControls(this.document.observations);
    this.document.verdict = verdict;
    this.document.generatedAt = this.now().toISOString();
    return this.document;
  }

  /** @param {string} path */
  write(path) {
    return writeRedactedEvidence(path, this.document);
  }
}
