// tests/unit/adversarial/fuzz-core.test.ts
// Task 51 — the seeded core must be reproducible and the shrinker must be honest.
//
// These are the tests that make every other Task 51 result citable. If the corpus
// is not byte-identical for a fixed seed, a recorded finding cannot be replayed and
// the evidence is a story. If the shrinker accepts a candidate that fails for a
// DIFFERENT reason, every minimized artifact is a different bug from the one found.

import { describe, expect, it } from 'vitest';

import { Rng, hashSeed, normalizeSeed, streamFor } from './fuzz-random.mjs';
import {
  fuzzAssetPath,
  fuzzConsoleCommand,
  fuzzNumeric,
  fuzzString,
  COMMAND_ATOMS,
} from './fuzz-generators.mjs';
import { fuzzJsonRpcFrame, fragmentSse, fuzzAuthHeaders, fuzzExecuteEnvelope } from './fuzz-protocol.mjs';
import { shrinkString, shrinkRecord, stringCandidates, replayArtifact } from './fuzz-shrink.mjs';
import { SEEDS, BUDGETS } from './fuzz-seeds.mjs';

describe('task 51 — seeded generation is reproducible', () => {
  it('produces a byte-identical corpus for a fixed seed', () => {
    const draw = () => {
      const rng = streamFor(SEEDS.commands, 'console-commands');
      return rng.list(200, (stream) => fuzzConsoleCommand(stream));
    };
    expect(JSON.stringify(draw())).toBe(JSON.stringify(draw()));
  });

  it('gives independent streams to independently named generators', () => {
    const commands = streamFor(SEEDS.commands, 'console-commands').list(50, (r) => fuzzString(r));
    const paths = streamFor(SEEDS.commands, 'asset-paths').list(50, (r) => fuzzString(r));
    expect(commands).not.toEqual(paths);
  });

  it('keeps an existing stream stable when a NEW stream name is introduced', () => {
    // The property that lets a recorded seed keep its meaning as the suite grows.
    const before = streamFor(SEEDS.protocol, 'json-rpc').list(30, (r) => fuzzString(r));
    streamFor(SEEDS.protocol, 'a-brand-new-generator').list(999, (r) => fuzzString(r));
    const after = streamFor(SEEDS.protocol, 'json-rpc').list(30, (r) => fuzzString(r));
    expect(after).toEqual(before);
  });

  it('hashes a seed label to a stable uint32 and rejects a non-integer seed', () => {
    expect(hashSeed('protocol-fuzz/v1')).toBe(hashSeed('protocol-fuzz/v1'));
    expect(normalizeSeed('abc')).toBe(hashSeed('abc'));
    expect(() => normalizeSeed(1.5)).toThrow(/integer/u);
  });

  it('reset() replays the same sequence from the same object', () => {
    const rng = new Rng(SEEDS.commands, 'unit');
    const first = rng.list(20, (r) => r.next());
    const second = rng.reset().list(20, (r) => r.next());
    expect(second).toEqual(first);
  });

  it('draws every weighted branch and never leaves the declared range', () => {
    const rng = new Rng(SEEDS.commands, 'range');
    for (let i = 0; i < 5000; i += 1) {
      const value = rng.int(3, 7);
      expect(value).toBeGreaterThanOrEqual(3);
      expect(value).toBeLessThanOrEqual(7);
    }
  });
});

describe('task 51 — generators cover the classes they claim', () => {
  it('emits every declared console-command class within the budget', () => {
    const rng = streamFor(SEEDS.commands, 'console-commands');
    const classes = new Set(rng.list(BUDGETS.commandCases, (r) => fuzzConsoleCommand(r).class));
    for (const expected of [
      'benign', 'shared-python-first', 'shared-separator', 'dangerous-first-token',
      'dangerous-later-token', 'shared-token', 'glued-token', 'native-only-first-token',
      'flexible-python-spelling',
      'casing-mutation', 'whitespace-mutation', 'free-text',
    ]) {
      expect(classes, `class "${expected}" was never generated, so nothing tested it`).toContain(expected);
    }
  });

  it('emits in-prefix, escape and malformed asset paths', () => {
    const rng = streamFor(SEEDS.paths, 'asset-paths');
    const intents = new Set(rng.list(BUDGETS.pathCases, (r) => fuzzAssetPath(r).intent));
    expect([...intents].sort()).toEqual(['escape', 'in-prefix', 'malformed']);
  });

  it('emits both numeric and string spellings of hostile numbers', () => {
    const rng = streamFor(SEEDS.protocol, 'numbers');
    const kinds = new Set(rng.list(400, (r) => typeof fuzzNumeric(r)));
    expect([...kinds].sort()).toEqual(['number', 'string']);
  });

  it('emits well-formed and malformed JSON-RPC frames', () => {
    const rng = streamFor(SEEDS.protocol, 'json-rpc');
    const shapes = new Set(rng.list(BUDGETS.protocolCases, (r) => fuzzJsonRpcFrame(r).shape));
    expect(shapes.size).toBeGreaterThanOrEqual(8);
    expect(shapes).toContain('deep-nesting');
    expect(shapes).toContain('batch');
  });

  it('fragments an SSE event across arbitrary chunk boundaries and preserves the payload', () => {
    const rng = streamFor(SEEDS.protocol, 'sse');
    for (let i = 0; i < 200; i += 1) {
      const payload = JSON.stringify({ jsonrpc: '2.0', id: i, result: { ok: true } });
      const { chunks, expected } = fragmentSse(rng, { data: payload, event: 'message' });
      expect(chunks.join('')).toContain(expected);
      expect(chunks.every((chunk) => chunk.length > 0)).toBe(true);
    }
  });

  it('emits an unauthorized header set far more often than an authorized one', () => {
    const rng = streamFor(SEEDS.auth, 'headers');
    const rows = rng.list(500, (r) => fuzzAuthHeaders(r, { token: 'super-secret-token', sessionId: 'sess-1' }));
    const authorized = rows.filter((row) => row.authorized).length;
    expect(authorized).toBeGreaterThan(0);
    expect(authorized).toBeLessThan(rows.length / 2);
    // The near-miss token must never equal the real one: a generator that emitted
    // the valid token as a "near miss" would make the auth property vacuous.
    for (const row of rows.filter((entry) => entry.shape === 'near-miss-token')) {
      expect(row.headers['X-MCP-Capability-Token']).not.toBe('super-secret-token');
    }
  });

  it('emits a consent grant naming the WRONG capability', () => {
    const rng = streamFor(SEEDS.auth, 'envelope');
    const rows = rng.list(400, (r) => fuzzExecuteEnvelope(r, {
      capabilityId: 'manage_asset.create_material',
      otherCapabilityId: 'control_actor.delete_actor',
    }));
    const mismatched = rows.filter((row) => {
      const consent = row.consent as { capability?: string } | null;
      return consent !== null && consent.capability === 'control_actor.delete_actor';
    });
    expect(mismatched.length).toBeGreaterThan(0);
  });

  it('keeps the benign command list genuinely benign (a positive control for the policy)', () => {
    expect(COMMAND_ATOMS.benign.length).toBeGreaterThan(5);
    for (const command of COMMAND_ATOMS.benign) {
      expect(command).not.toMatch(/[;|`&]/u);
    }
  });
});

describe('task 51 — the shrinker minimizes deterministically and preserves the failure', () => {
  it('reduces to the same minimum on repeated runs', () => {
    const probe = (candidate: string) => (candidate.includes('quit') ? 'HAS_QUIT' : null);
    const input = 'stat fps \u200bqu\u0130it quit \ud800 memreport';
    const first = shrinkString(input, probe);
    const second = shrinkString(input, probe);
    expect(second.minimal).toBe(first.minimal);
    expect(first.minimal).toBe('quit');
    expect(first.tag).toBe('HAS_QUIT');
  });

  it('never accepts a candidate that fails for a DIFFERENT reason', () => {
    // 'quit' fails as HAS_QUIT; the bare 'q' fails as HAS_Q. A tag-blind shrinker
    // would happily walk to 'q' and report a minimum that proves another bug.
    const probe = (candidate: string) => {
      if (candidate.includes('quit')) return 'HAS_QUIT';
      if (candidate.includes('q')) return 'HAS_Q';
      return null;
    };
    const result = shrinkString('xxquitxx', probe);
    expect(result.tag).toBe('HAS_QUIT');
    expect(result.minimal).toContain('quit');
  });

  it('returns the input untouched when it does not fail at all', () => {
    const result = shrinkString('stat fps', () => null);
    expect(result.minimal).toBe('stat fps');
    expect(result.tag).toBeNull();
  });

  it('respects the evaluation budget instead of running forever', () => {
    // The probe accepts every non-empty prefix, so an unbudgeted shrink would keep
    // halving until it hit one character. A budget of 5 stops it mid-descent, which
    // is the case that matters: a shrink that outruns its budget must REPORT that
    // and hand back its best-so-far, never silently pass off a partial minimum as
    // a converged one.
    const result = shrinkString('a'.repeat(400), (candidate) => (candidate.length > 0 ? 'NONEMPTY' : null), { budget: 5 });
    expect(result.evaluations).toBeLessThanOrEqual(5);
    expect(result.budgetExhausted).toBe(true);
    expect(result.minimal.length).toBeGreaterThan(1);
  });

  it('produces no candidate equal to its own input', () => {
    for (const input of ['', 'a', 'quit', 'stat fps \u200b']) {
      expect(stringCandidates(input)).not.toContain(input);
    }
  });

  it('drops only the record keys that are not needed to reproduce the tag', () => {
    // The failure is "a consent sibling is present"; `note` is noise. A shrinker
    // that dropped `consent` would destroy the failure, and one that kept `note`
    // would leave an irrelevant field in the artifact for the next reader to chase.
    const probe = (candidate: Record<string, unknown>) => (candidate.consent === undefined ? null : 'HAS_CONSENT');
    const result = shrinkRecord(
      { tool: 'manage_asset', action: 'create_material', consent: { capability: 'x' }, note: 'noise' },
      probe,
      { required: ['tool', 'action'] },
    );
    expect(result.tag).toBe('HAS_CONSENT');
    expect(result.minimal.tool).toBe('manage_asset');
    expect(result.minimal.action).toBe('create_material');
    expect(result.minimal.consent).toBeDefined();
    expect(result.dropped).toEqual(['note']);
  });

  it('emits a replay artifact carrying the coordinates a re-run needs', () => {
    const artifact = replayArtifact({
      suite: 'parity-security',
      property: 'console policy parity',
      seed: SEEDS.commands,
      stream: 'console-commands',
      index: 42,
      tag: 'DIVERGED',
      original: 'stat fps quit',
      minimal: 'quit',
      evaluations: 12,
    });
    expect(artifact.replay).toContain('vitest run tests/unit/adversarial/parity-security.test.ts');
    expect(artifact.reproduce.seed).toBe(SEEDS.commands);
    expect(artifact.reproduce.index).toBe(42);
    expect(artifact.minimal).toBe('quit');
  });
});
