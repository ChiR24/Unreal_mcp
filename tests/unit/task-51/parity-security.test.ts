// tests/unit/task-51/parity-security.test.ts
// Task 51 — the fail-closed RUNTIME security properties, driven with seeded hostile
// input against the real objects.
//
// Scope boundary, so neither file re-tests the other: the console-command
// differential and everything about its allowlist live in differential-command.test.ts.
// This file owns the native mirror's fidelity to the compiled C++, path and symlink
// containment, the idempotency ledger under interleaving, and settle-once
// cancellation.
//
// Not here on purpose: the plugin's constant-time token compare and its fail-closed
// non-loopback bind. Those are pinned by tests/unit/plugin/security_contracts.test.ts,
// which reads the C++ and asserts the required and forbidden patterns. Re-asserting
// the same strings would add a second place to update and no new detection. What
// this file adds instead is the coverage a source check cannot give: a rule can be
// present in the source and still be walked past at run time.

import { afterAll, describe, expect, it } from 'vitest';

import { streamFor } from './fuzz-random.mjs';
import { fuzzAssetPath } from './fuzz-generators.mjs';
import { firstTokenWs, loadNativePolicy, nativeDecision, verifyNativeAlgorithmContract } from './native-policy-mirror.mjs';
import {
  hostPathContained,
  makeSymlinkFixture,
  normalizedPathIsContained,
  settleOnce,
} from './security-properties.mjs';
import { exerciseIdempotencyLedger, uePathVerdict } from './security-properties-source.mjs';
import { BUDGETS, SEEDS } from './fuzz-seeds.mjs';

const policy = loadNativePolicy(process.cwd());

describe('Task 51 — the native mirror is derived from the plugin, not from memory', () => {
  it('parses every policy array out of the generated header', () => {
    expect(policy.separators).toContain(';');
    expect(policy.blocked).toContain('shutdown');
    expect(policy.restricted).toContain('ubt');
    expect(policy.forbiddenNames).toContain('rm');
    expect(policy.forbiddenTokens).toContain('os.system');
  });

  it('throws rather than returning an empty policy when the header is unreadable', () => {
    // An empty block list would make the mirror permissive, and every parity
    // assertion built on it would then pass for the wrong reason.
    expect(() => loadNativePolicy('/nonexistent-root-for-task-51')).toThrow();
  });

  it('still matches the algorithm the plugin compiles', () => {
    const contract = verifyNativeAlgorithmContract(process.cwd());
    expect(contract.missing, `IsBlockedCommand no longer contains: ${contract.missing.join(', ')}`).toEqual([]);
    expect(contract.ok).toBe(true);
  });

  it('reproduces ParseIntoArrayWS, including a leading-whitespace input', () => {
    // ParseIntoArrayWS drops empty entries; a naive split leaves '' first and would
    // let "  quit" through as an empty command name.
    expect(firstTokenWs('   quit now')).toBe('quit');
    expect(nativeDecision('   quit now', policy).blocked).toBe(true);
  });

  it('declines to adjudicate non-ASCII input instead of guessing UE case folding', () => {
    expect(nativeDecision('QUİT', policy).decidable).toBe(false);
    expect(nativeDecision('quit', policy).decidable).toBe(true);
  });
});

describe('Task 51 — path and symlink containment under seeded hostile input', () => {
  const fixture = makeSymlinkFixture();
  afterAll(() => { fixture.dispose(); });

  it('never returns an accepted UE path that escapes its root', () => {
    const rng = streamFor(SEEDS.paths, 'asset-paths');
    const failures: string[] = [];
    let accepted = 0;
    for (let index = 0; index < BUDGETS.pathCases; index += 1) {
      const testCase = fuzzAssetPath(rng);
      const verdict = uePathVerdict(testCase.path);
      if (!verdict.accepted) continue;
      accepted += 1;
      const contained = normalizedPathIsContained(String(verdict.normalized));
      if (!contained.ok) {
        failures.push(`${JSON.stringify(testCase.path)} -> ${String(verdict.normalized)}: ${contained.why}`);
      }
    }
    // The setup must prove non-empty: a run where nothing was accepted would satisfy
    // the assertion below while having exercised no containment at all.
    expect(accepted, 'no path was accepted, so containment was never exercised').toBeGreaterThan(50);
    expect(failures).toEqual([]);
  });

  it('rejects every traversal spelling the generator can emit', () => {
    for (const spelling of [
      '/Game/../Secret', '/Game/..\\Secret', '/Game/....//Secret',
      '/Game/a/../../b', '/Game/.../.../x',
    ]) {
      expect(uePathVerdict(spelling).accepted, spelling).toBe(false);
    }
  });

  it('rejects a path outside every allowed root, including near-miss roots', () => {
    for (const spelling of ['/Games/Thing', '/Gam/Thing', '/Content/Thing', 'Game/Thing', '/GameX/Thing']) {
      expect(uePathVerdict(spelling).accepted, spelling).toBe(false);
    }
    // Positive control: the real roots must still be accepted, or the assertion
    // above is satisfied by a function that rejects everything.
    for (const spelling of ['/Game/Thing', '/Engine/Thing', '/Niagara/Thing']) {
      expect(uePathVerdict(spelling).accepted, spelling).toBe(true);
    }
  });

  it('catches a REAL symlink that is textually inside the owned root and resolves outside it', () => {
    const escaping = hostPathContained(fixture.owned, fixture.escapingPath);
    expect(escaping.decided).toBe(true);
    expect(escaping.contained, escaping.why).toBe(false);
    // Positive control: without it, a predicate that always answered "outside"
    // would satisfy the assertion above and detect nothing.
    const contained = hostPathContained(fixture.owned, fixture.containedPath);
    expect(contained.decided).toBe(true);
    expect(contained.contained, contained.why).toBe(true);
    // A symlink that stays inside the owned root must NOT be refused.
    const loopback = hostPathContained(fixture.owned, fixture.loopbackLinkPath);
    expect(loopback.contained, loopback.why).toBe(true);
  });

  it('reports a non-existent candidate as undecided rather than contained', () => {
    const missing = hostPathContained(fixture.owned, `${fixture.owned}/never-created`);
    expect(missing.decided).toBe(false);
    expect(missing.contained).toBe(false);
  });
});

describe('Task 51 — the idempotency ledger under seeded interleaving', () => {
  it('breaks none of its invariants across independent seeded runs', () => {
    const failures: string[] = [];
    for (let run = 0; run < BUDGETS.ledgerRuns; run += 1) {
      const rng = streamFor(SEEDS.ledger, `ledger-run-${run}`);
      const outcome = exerciseIdempotencyLedger(rng, {
        operations: BUDGETS.ledgerOperations,
        maxEntries: 8,
      });
      for (const violation of outcome.violations) failures.push(`run ${run}: ${violation}`);
    }
    expect(failures).toEqual([]);
  });

  it('leaves slots in flight, so the never-evict-in-flight invariant was actually tested', () => {
    const rng = streamFor(SEEDS.ledger, 'ledger-run-0');
    const outcome = exerciseIdempotencyLedger(rng, { operations: BUDGETS.ledgerOperations, maxEntries: 8 });
    expect(outcome.inFlight, 'no slot was left in flight, so nothing exercised eviction').toBeGreaterThan(0);
  });

  it('drives the ledger past its cap, so eviction actually ran', () => {
    const rng = streamFor(SEEDS.ledger, 'ledger-pressure');
    const outcome = exerciseIdempotencyLedger(rng, { operations: 600, maxEntries: 4 });
    expect(outcome.violations).toEqual([]);
    expect(outcome.size).toBeGreaterThan(0);
  });

  it('never puts a raw idempotency key in its diagnostic state', () => {
    const rng = streamFor(SEEDS.ledger, 'ledger-secrecy');
    const outcome = exerciseIdempotencyLedger(rng, { operations: 200, maxEntries: 4 });
    for (const key of ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']) {
      expect(outcome.debugState, `debugState leaked the raw key ${key}`).not.toContain(`"${key}"`);
    }
    // Positive control: the digests ARE present, so the assertion above is not
    // satisfied by an empty diagnostic string.
    expect(outcome.debugState.length).toBeGreaterThan(2);
  });
});

describe('Task 51 — a cancellation race settles exactly once', () => {
  it('delivers one terminal result no matter how many arrive', () => {
    const rng = streamFor(SEEDS.protocol, 'cancel-race');
    for (let run = 0; run < 500; run += 1) {
      const box = settleOnce();
      const contenders = rng.shuffle(['result', 'cancelled', 'timeout', 'late-result'])
        .slice(0, rng.int(2, 4));
      for (const contender of contenders) box.settle(contender);
      expect(box.state.delivered).toBe(1);
      expect(box.state.attempted).toBeGreaterThanOrEqual(2);
      // The FIRST arrival wins; a late response must never overwrite a settled one.
      expect(box.state.value).toBe(contenders[0]);
    }
  });

  it('reports a refused late settle rather than silently dropping it', () => {
    const box = settleOnce();
    expect(box.settle('first')).toBe(true);
    expect(box.settle('late')).toBe(false);
    expect(box.state).toEqual({ attempted: 2, delivered: 1, value: 'first' });
  });
});
