// tests/unit/evidence-oracles/evidence-validator.test.ts
// Task 50 — the evidence validator, proven by making it REFUSE.
//
// The six rejections the plan requires are each tested by taking a document the
// validator ACCEPTS and changing exactly ONE thing. That shape matters: a
// validator that refused every document would pass all six refusal tests, so the
// first test here is the positive control, and every refusal test is a one-field
// delta from it. If the positive control ever breaks, every refusal below becomes
// meaningless and the suite says so loudly rather than staying green.
//
// Real files, real hashes, real pids. A mocked fs would let a "stale package"
// check pass against timestamps no build system ever produces.

import { mkdirSync, readFileSync, rmSync, utimesSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { INDEPENDENCE } from './state-oracles.mjs';
import {
  REJECTIONS,
  describeRejections,
  snapshotArtifact,
  snapshotTree,
  treeDigestOf,
  validateEvidence,
} from './evidence-validator.mjs';

const ROOT = join('/tmp/opencode/task-50', `evidence-spec-${process.pid}`);

/** Codes present in a result, for a compact assertion. */
const codes = (result: ReturnType<typeof validateEvidence>) => result.rejections.map((entry) => entry.code);

/**
 * A document that VALIDATES. Every refusal test below is this, minus one field.
 * Built from real files so the hashes and timestamps are the ones the filesystem
 * actually produces.
 */
function validDocument(): Record<string, unknown> {
  const source = join(ROOT, 'src/module.ts');
  mkdirSync(join(ROOT, 'src'), { recursive: true });
  writeFileSync(source, 'export const answer = 42;\n', { mode: 0o600 });
  const artifact = join(ROOT, 'dist/module.js');
  mkdirSync(join(ROOT, 'dist'), { recursive: true });
  writeFileSync(artifact, 'export const answer = 42;\n', { mode: 0o600 });
  // The build is NEWER than its input, as a fresh build is.
  const built = Date.now() / 1000 + 60;
  utimesSync(artifact, built, built);

  const tree = snapshotTree({ projectRoot: ROOT, files: ['src/module.ts'] });
  const built1 = snapshotArtifact({
    projectRoot: ROOT, path: 'dist/module.js',
    inputsNewest: 'src/module.ts', inputsNewestAtMs: Date.now() - 60_000,
  });

  return {
    task: 50,
    title: 'oracle + evidence spec fixture',
    plan: '.omo/plans/pure-unreal-mcp-implementation.md',
    kind: 'spec fixture',
    generatedAt: new Date().toISOString(),
    verdict: 'fixture',
    environment: {
      mockUnrealConnection: false,
      processes: [{ pid: process.pid, role: 'harness', startTicks: currentStartTicks(), comm: 'node', cmdlinePreview: 'node', aliveAtCapture: true, observedAt: new Date().toISOString() }],
    },
    tree,
    artifacts: [built1],
    engine: { engineRoot: '/data/UnrealEngine', version: '5.7.4' },
    clients: [{ id: 'native', transport: 'http-sse', protocolVersion: '2025-06-18' }],
    commands: [{ cmd: 'npx vitest run', exitCode: 0 }],
    transcripts: [{ id: 'tx-1', transport: 'native', request: '{}', response: '{}' }],
    observations: [
      { id: 'obs-pre', phase: 'pre', kind: 'asset', mechanism: 'fs:uasset-package', independence: INDEPENDENCE.OUT_OF_BAND, target: '/Game/MCPTest/run/M_A', present: false, digest: null, conclusive: true, detail: {}, observedAt: new Date().toISOString() },
      { id: 'obs-post', phase: 'post', kind: 'asset', mechanism: 'fs:uasset-package', independence: INDEPENDENCE.OUT_OF_BAND, target: '/Game/MCPTest/run/M_A', present: true, digest: 'a'.repeat(64), conclusive: true, detail: {}, observedAt: new Date().toISOString() },
      { id: 'obs-clean', phase: 'cleanup', kind: 'namespace', mechanism: 'fs:tree-digest', independence: INDEPENDENCE.OUT_OF_BAND, target: '/x', present: false, digest: 'b'.repeat(64), conclusive: true, detail: {}, observedAt: new Date().toISOString() },
    ],
    claims: [{
      id: 'claim-create', target: '/Game/MCPTest/run/M_A', effect: 'created', outcome: 'success',
      verdict: 'PROVEN', pass: true, reason: 'observed absent then present',
      oracleRefs: ['obs-pre', 'obs-post'], cleanupRef: 'cleanup-1', transcriptRef: 'tx-1',
    }],
    cleanup: [{ id: 'cleanup-1', owned: '/Game/MCPTest/run', verifiedBy: 'obs-clean', pass: true, verdict: 'PROVEN', reason: 'digest restored' }],
    positiveControls: { ok: true, mechanisms: [{ kind: 'asset', mechanism: 'fs:uasset-package', sawPresent: true, sawAbsent: true, inconclusive: 0 }], missing: [] },
    notProven: [],
    notes: [],
  };
}

/** Read this process's own start time the same way the validator will re-read it. */
function currentStartTicks(): number {
  const stat = readFileSync(`/proc/${process.pid}/stat`, 'utf8');
  return Number(stat.slice(stat.lastIndexOf(')') + 2).split(' ')[19]);
}

beforeAll(() => { mkdirSync(ROOT, { recursive: true }); });
afterAll(() => { rmSync(ROOT, { recursive: true, force: true }); });

describe('Task 50 — POSITIVE CONTROL: a well-formed document validates', () => {
  it('accepts a document whose every hash, build, pid, oracle and cleanup link re-checks', () => {
    const result = validateEvidence(validDocument(), { projectRoot: ROOT });
    expect(result.rejections).toEqual([]);
    expect(result.valid).toBe(true);
    expect(describeRejections(result)).toContain('evidence VALID');
  });

  it('actually inspected things — a validator that checked nothing would also report valid', () => {
    const result = validateEvidence(validDocument(), { projectRoot: ROOT });
    expect(result.checked).toMatchObject({ artifacts: 1, processes: 1, observations: 3, claims: 1, cleanup: 1, treeFiles: 1 });
  });
});

describe('Task 50 — REQUIRED REJECTION 1: a stale tree', () => {
  it('REJECTS when a recorded source file no longer hashes the same', () => {
    const document = validDocument();
    writeFileSync(join(ROOT, 'src/module.ts'), 'export const answer = 43; // edited after the run\n', { mode: 0o600 });
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.STALE_TREE);
    expect(describeRejections(result)).toContain('cannot be reported as observations of this one');
  });

  it('REJECTS when a recorded source file has been deleted', () => {
    const document = validDocument();
    rmSync(join(ROOT, 'src/module.ts'));
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.STALE_TREE);
  });

  it('REJECTS a document that recorded NO source files, because staleness would be undetectable', () => {
    const document = validDocument();
    document.tree = { files: [], sourceDigest: treeDigestOf([]) };
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.STALE_TREE);
  });

  it('REJECTS a digest that does not summarise its own file list', () => {
    const document = validDocument();
    (document.tree as Record<string, unknown>).sourceDigest = 'f'.repeat(64);
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.MALFORMED);
  });
});

describe('Task 50 — REQUIRED REJECTION 2: a stale package', () => {
  it('REJECTS an artifact built BEFORE its newest input — the Task 46 stale-dist failure', () => {
    const document = validDocument();
    const artifact = (document.artifacts as Record<string, unknown>[])[0];
    artifact.inputsNewestAtMs = Number(artifact.builtAtMs) + 300_000;
    artifact.inputsNewest = 'src/module.ts';
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.STALE_PACKAGE);
    expect(describeRejections(result)).toContain('measured the BUILD, not the tree');
  });

  it('REJECTS an artifact that records no build/input timestamps at all', () => {
    const document = validDocument();
    const artifact = (document.artifacts as Record<string, unknown>[])[0];
    delete artifact.inputsNewestAtMs;
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.STALE_PACKAGE);
  });

  it('REJECTS a document naming no artifact at all', () => {
    const document = validDocument();
    document.artifacts = [];
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.MISSING_FIELD);
  });
});

describe('Task 50 — REQUIRED REJECTION 3: a stale PID', () => {
  it('REJECTS a process recorded without a start time — a bare pid can never be re-checked', () => {
    const document = validDocument();
    delete ((document.environment as Record<string, unknown>).processes as Record<string, unknown>[])[0].startTicks;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.STALE_PID);
    expect(describeRejections(result)).toContain('the kernel recycles pids');
  });

  it('REJECTS a live pid whose start time no longer matches — the record now names someone else', () => {
    const document = validDocument();
    const entry = ((document.environment as Record<string, unknown>).processes as Record<string, unknown>[])[0];
    entry.startTicks = Number(entry.startTicks) + 1;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.STALE_PID);
    expect(describeRejections(result)).toContain('a different process');
  });

  it('POSITIVE CONTROL: a pid that has since EXITED is not a stale record — nothing contradicts it', () => {
    const document = validDocument();
    ((document.environment as Record<string, unknown>).processes as Record<string, unknown>[])[0] = {
      pid: 999_999, role: 'exited child', startTicks: 12_345, aliveAtCapture: true,
    };
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).not.toContain(REJECTIONS.STALE_PID);
  });
});

describe('Task 50 — REQUIRED REJECTION 4: a bad hash', () => {
  it('REJECTS an artifact whose bytes no longer match the recorded hash', () => {
    const document = validDocument();
    writeFileSync(join(ROOT, 'dist/module.js'), 'export const answer = 999; // replaced\n', { mode: 0o600 });
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.BAD_HASH);
  });

  it('REJECTS an artifact that has vanished, rather than skipping the check', () => {
    const document = validDocument();
    rmSync(join(ROOT, 'dist/module.js'));
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.BAD_HASH);
  });
});

describe('Task 50 — REQUIRED REJECTION 5: a missing oracle link', () => {
  it('REJECTS a mutation claim that cites no oracle observation', () => {
    const document = validDocument();
    (document.claims as Record<string, unknown>[])[0].oracleRefs = [];
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.MISSING_ORACLE_LINK);
    expect(describeRejections(result)).toContain('a response is not proof of itself');
  });

  it('REJECTS a claim citing an observation id that does not exist', () => {
    const document = validDocument();
    (document.claims as Record<string, unknown>[])[0].oracleRefs = ['obs-pre', 'obs-imaginary'];
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.MISSING_ORACLE_LINK);
  });

  it('REJECTS a mutation with NO pre-state observation', () => {
    const document = validDocument();
    (document.claims as Record<string, unknown>[])[0].oracleRefs = ['obs-post'];
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.MISSING_ORACLE_LINK);
    expect(describeRejections(result)).toContain('cannot distinguish this call from a leftover');
  });

  it('REJECTS a mutation proven only by a same-plugin read — the Task 49 dependent oracle', () => {
    const document = validDocument();
    for (const entry of document.observations as Record<string, unknown>[]) {
      if (entry.id !== 'obs-clean') entry.independence = INDEPENDENCE.CROSS_TRANSPORT;
    }
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.DEPENDENT_ORACLE);
    expect(describeRejections(result)).toContain('wrong for the same reason');
  });

  it('POSITIVE CONTROL: a READ-only claim needs no out-of-band reading', () => {
    const document = validDocument();
    document.claims = [{
      id: 'claim-read', target: '/Game', effect: 'unchanged', outcome: 'success',
      verdict: 'PROVEN', pass: true, reason: 'nothing moved',
      oracleRefs: ['obs-post'], cleanupRef: null, transcriptRef: 'tx-1',
    }];
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).not.toContain(REJECTIONS.DEPENDENT_ORACLE);
    expect(result.valid).toBe(true);
  });
});

describe('Task 50 — REQUIRED REJECTION 6: a missing cleanup link', () => {
  it('REJECTS a claim that created owned state and cites no cleanup receipt', () => {
    const document = validDocument();
    (document.claims as Record<string, unknown>[])[0].cleanupRef = null;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.MISSING_CLEANUP_LINK);
    expect(describeRejections(result)).toContain('changes the result of the NEXT run');
  });

  it('POSITIVE CONTROL: a REFUSED create whose post-state shows nothing exists needs no receipt', () => {
    const document = validDocument();
    // The refusal case: labelled `created` because that is what was attempted,
    // but the oracle read the target absent afterwards, so there is no fixture.
    for (const entry of document.observations as Record<string, unknown>[]) {
      if (entry.phase === 'post') entry.present = false;
    }
    (document.claims as Record<string, unknown>[])[0].outcome = 'error';
    (document.claims as Record<string, unknown>[])[0].cleanupRef = null;
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).not.toContain(REJECTIONS.MISSING_CLEANUP_LINK);
  });

  it('a claim CANNOT relabel its way out of a leak the oracle can see', () => {
    const document = validDocument();
    // Post-state still shows the asset present; only the label was softened.
    (document.claims as Record<string, unknown>[])[0].outcome = 'error';
    (document.claims as Record<string, unknown>[])[0].cleanupRef = null;
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.MISSING_CLEANUP_LINK);
  });

  it('REJECTS a claim citing a cleanup receipt that did not pass', () => {
    const document = validDocument();
    (document.cleanup as Record<string, unknown>[])[0].pass = false;
    (document.cleanup as Record<string, unknown>[])[0].reason = 'two materials survived';
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.UNVERIFIED_CLEANUP);
    expect(describeRejections(result)).toContain('two materials survived');
  });

  it('REJECTS a cleanup receipt that cites no post-cleanup observation — the cleanupClean:true shape', () => {
    const document = validDocument();
    delete (document.cleanup as Record<string, unknown>[])[0].verifiedBy;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.UNVERIFIED_CLEANUP);
    expect(describeRejections(result)).toContain('a delete response is the claim, not the proof');
  });

  it('REJECTS a cleanup receipt whose observation id does not resolve', () => {
    const document = validDocument();
    (document.cleanup as Record<string, unknown>[])[0].verifiedBy = 'obs-nonexistent';
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.UNVERIFIED_CLEANUP);
  });
});

describe('Task 50 — the rejections beyond the six required ones', () => {
  it('REJECTS a document produced in mock mode', () => {
    const document = validDocument();
    (document.environment as Record<string, unknown>).mockUnrealConnection = true;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.MOCK_EVIDENCE);
    expect(describeRejections(result)).toContain('must never be recorded as one');
  });

  it('REJECTS a document with no positive control — a blind oracle satisfies every absence assertion', () => {
    const document = validDocument();
    document.positiveControls = { ok: false, mechanisms: [], missing: ['fs:uasset-package never saw a present reading'] };
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toContain(REJECTIONS.NO_POSITIVE_CONTROL);
  });

  it('REJECTS a leaked capability token value anywhere in the document', () => {
    const document = validDocument();
    document.notes = ['the run used token supersecrettoken1234'];
    const result = validateEvidence(document, { projectRoot: ROOT, env: { MCP_QA_TOKEN: 'supersecrettoken1234' } });
    expect(codes(result)).toContain(REJECTIONS.SECRET_LEAK);
  });

  it('REJECTS an unrecognised top-level key, because a typo silently disables the check it feeds', () => {
    const document = validDocument();
    (document as Record<string, unknown>).observation = document.observations;
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.UNKNOWN_FIELD);
  });

  it('REJECTS a missing required section by name', () => {
    const document = validDocument();
    delete document.cleanup;
    const result = validateEvidence(document, { projectRoot: ROOT });
    expect(codes(result)).toEqual([REJECTIONS.MISSING_FIELD]);
    expect(result.rejections[0].at).toBe('/cleanup');
  });

  it('REJECTS a non-object document', () => {
    expect(codes(validateEvidence('not evidence', { projectRoot: ROOT }))).toEqual([REJECTIONS.MALFORMED]);
  });

  it('REJECTS an observation that does not state its mechanism or independence', () => {
    const document = validDocument();
    delete (document.observations as Record<string, unknown>[])[1].independence;
    expect(codes(validateEvidence(document, { projectRoot: ROOT }))).toContain(REJECTIONS.MALFORMED);
  });
});
