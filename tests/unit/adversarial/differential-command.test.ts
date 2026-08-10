// tests/unit/adversarial/differential-command.test.ts
// Task 51 — the console-command differential, and the tests that keep its
// allowlist from becoming a transcript of the bugs.
//
// There are two suites here and the second one is the important one.
//
// The first runs the differential: a seeded corpus of console commands is put to
// the real TypeScript evaluator and to the native mirror, and every disagreement
// must be attributable to a declared, argued asymmetry class.
//
// The second attacks the allowlist itself. A differential whose excuse list can
// absorb anything is not a test, and the failure is invisible from the inside:
// the suite still runs, still reports "0 findings", and is permanently blind. So
// every rejection rule in `validateAllowlist` gets a case that proves it actually
// rejects, and the engine gets cases proving an unattributable or wrong-direction
// divergence still surfaces even when a plausible-looking class exists.
//
// The distinction the whole file turns on: an allowlist entry may name RULES and a
// DIRECTION. It may never name an input, a seed, or a count. A rule pair is a
// design decision that can be reviewed; a list of inputs is a list of bugs.

import { describe, expect, it } from 'vitest';

import {
  COMMAND_ASYMMETRY_CLASSES,
  NATIVE_REASONS,
  explainCommandDivergence,
  nativeSideFactory,
} from './command-parity.mjs';
import { KNOWN_RULE_IDS, bothMustRefuse, typescriptSide } from './command-parity-source.mjs';
import { runDifferential, validateAllowlist } from './differential-engine.mjs';
import type { AsymmetryClass, SideResult } from './differential-engine.mjs';
import { COMMAND_ATOMS, fuzzConsoleCommand } from './fuzz-generators.mjs';
import { streamFor } from './fuzz-random.mjs';
import { loadNativePolicy, nativeDecision, verifyNativeAlgorithmContract } from './native-policy-mirror.mjs';
import { replayArtifact, shrinkString } from './fuzz-shrink.mjs';
import { BUDGETS, SEEDS } from './fuzz-seeds.mjs';

/** Pinned centrally in fuzz-seeds.mjs so every suite's seed lives in one file and a
 * recorded finding names a value someone can look up rather than one buried here. */
const SEED = SEEDS.commands;
/** Large enough that every declared asymmetry class is actually exercised. */
const CORPUS_SIZE = BUDGETS.commandCases;

const policy = loadNativePolicy();
const nativeSide = nativeSideFactory(policy);

/** Build the seeded corpus. A pure function of the seed, so two runs are identical. */
function buildCommandCorpus(seed: string | number, size: number) {
  const rng = streamFor(seed, 'console-commands');
  return rng.list(size, (stream, index) => {
    const generated = fuzzConsoleCommand(stream);
    return { input: generated.command, label: generated.class, index };
  });
}

describe('Task 51 — console-command differential (TypeScript vs the plugin)', () => {
  it('the mirror still describes the C++ that is actually compiled', () => {
    // If this fails the mirror is describing code that no longer exists, and every
    // parity result below is a statement about a fiction.
    const contract = verifyNativeAlgorithmContract();
    expect(contract.missing).toEqual([]);
    expect(contract.ok).toBe(true);
  });

  it('the declared asymmetry classes are structurally legitimate', () => {
    const verdict = validateAllowlist(COMMAND_ASYMMETRY_CLASSES, KNOWN_RULE_IDS, NATIVE_REASONS);
    expect(verdict.problems).toEqual([]);
    expect(verdict.ok).toBe(true);
  });

  it('every disagreement over the seeded corpus is attributable to a declared class', () => {
    const cases = buildCommandCorpus(SEED, CORPUS_SIZE);
    const result = runDifferential({
      name: 'console-command-policy',
      cases,
      left: typescriptSide,
      right: nativeSide,
      explain: explainCommandDivergence,
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });

    // A finding is minimized before it is reported, so the failure message names a
    // short input rather than 60 characters of zero-width joiners.
    if (result.findings.length > 0) {
      const first = result.findings[0] as Record<string, unknown>;
      const command = String(first.input);
      const probe = (candidate: string): string | null => {
        const left = typescriptSide(candidate);
        const right = nativeSide(candidate);
        if (left.verdict === right.verdict) return null;
        if (left.verdict === 'undecidable' || right.verdict === 'undecidable') return null;
        if (explainCommandDivergence(candidate, left, right) !== null) return null;
        return `${left.verdict}/${right.verdict}`;
      };
      const shrunk = shrinkString(command, probe);
      const artifact = replayArtifact({
        suite: 'differential-command',
        property: 'every disagreement over the seeded corpus is attributable to a declared class',
        seed: SEED,
        stream: 'console-commands',
        index: Number(first.index),
        tag: String(shrunk.tag ?? first.direction),
        original: command,
        minimal: shrunk.minimal,
        evaluations: shrunk.evaluations,
        detail: { left: first.left, right: first.right, why: first.why },
      });
      expect.fail(`unattributed command divergence:\n${JSON.stringify(artifact, null, 2)}`);
    }

    expect(result.findings).toEqual([]);
    // Sanity: the corpus must actually reach both verdicts, or "0 findings" is the
    // trivial result of a generator that only ever produced accepts.
    expect(result.agreed).toBeGreaterThan(0);
    expect(result.total).toBe(CORPUS_SIZE);
  });

  it('every declared asymmetry class is load-bearing — none is a stale excuse', () => {
    // An allowlist entry that excuses nothing is either dead or, worse, a
    // pre-emptive excuse for a divergence nobody has seen yet. Both should be
    // deleted rather than carried, so an unused class fails the suite.
    const result = runDifferential({
      name: 'console-command-policy',
      cases: buildCommandCorpus(SEED, CORPUS_SIZE),
      left: typescriptSide,
      right: nativeSide,
      explain: explainCommandDivergence,
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(result.unusedClasses).toEqual([]);
  });

  it('is deterministic: the same seed produces byte-identical cases and results', () => {
    const first = buildCommandCorpus(SEED, 400);
    const second = buildCommandCorpus(SEED, 400);
    expect(second).toEqual(first);

    const run = () => runDifferential({
      name: 'console-command-policy',
      cases: first,
      left: typescriptSide,
      right: nativeSide,
      explain: explainCommandDivergence,
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(JSON.stringify(run())).toBe(JSON.stringify(run()));
  });
});

describe('Task 51 — the invariant no asymmetry class may excuse', () => {
  it('both surfaces refuse every dangerous first token', () => {
    // This is checked OUTSIDE the differential precisely so no allowlist entry can
    // reach it. The differential asks "is this difference excusable?"; this asks
    // "is the executable spelling refused by both?", which has no excusable form.
    const failures: string[] = [];
    for (const name of COMMAND_ATOMS.nativeFirstToken) {
      for (const spelling of [name, `${name} 1`, `  ${name}  arg`, name.toUpperCase()]) {
        const verdict = bothMustRefuse(spelling, policy);
        if (!verdict.ok) failures.push(`${JSON.stringify(spelling)}: ${verdict.reason}`);
      }
    }
    expect(failures).toEqual([]);
  });

  it('both surfaces refuse python as a first token', () => {
    const failures: string[] = [];
    for (const name of COMMAND_ATOMS.sharedPython) {
      for (const spelling of [`${name} print(1)`, `${name.toUpperCase()} x`, ` ${name} -c y`]) {
        const verdict = bothMustRefuse(spelling, policy);
        if (!verdict.ok) failures.push(`${JSON.stringify(spelling)}: ${verdict.reason}`);
      }
    }
    expect(failures).toEqual([]);
  });

  it('both surfaces refuse every command-chaining separator, wherever it appears', () => {
    // Separators are what would turn "first token only" from a precise rule into a
    // bypass, so this is the assumption the whole TS_ANYWHERE class rests on.
    const failures: string[] = [];
    for (const separator of COMMAND_ATOMS.sharedSeparators) {
      for (const spelling of [
        `stat fps${separator}quit`,
        `${separator}quit`,
        `stat fps ${separator} quit`,
      ]) {
        const verdict = bothMustRefuse(spelling, policy);
        if (!verdict.ok) failures.push(`${JSON.stringify(spelling)}: ${verdict.reason}`);
      }
    }
    expect(failures).toEqual([]);
  });

  it('benign commands stay usable on both surfaces', () => {
    // Without this, a policy that blocks everything would pass every test above.
    const wrongly: string[] = [];
    for (const command of COMMAND_ATOMS.benign) {
      const ts = typescriptSide(command);
      const native = nativeDecision(command, policy);
      if (ts.verdict !== 'accept') wrongly.push(`TypeScript blocked benign "${command}" (${ts.reason})`);
      if (native.blocked) wrongly.push(`the plugin blocked benign "${command}" (${native.reason})`);
    }
    expect(wrongly).toEqual([]);
  });
});

describe('Task 51 — anti-laundering: the allowlist cannot absorb an unexplained result', () => {
  const sound: AsymmetryClass = {
    id: 'SOUND',
    leftRules: [KNOWN_RULE_IDS[0] as string],
    rightReasons: ['ALLOWED'],
    direction: 'left-stricter',
    rationale: 'A rationale long enough to be a reviewed judgement rather than a placeholder string.',
    securityArgument: 'A security argument long enough to be a reviewed judgement rather than a placeholder.',
  };

  it('rejects an entry citing a rule the policy does not define', () => {
    // This is the shape an excuse takes when it is written to silence a result:
    // the author had a divergence, not a design decision, so the cited rule is
    // invented.
    const verdict = validateAllowlist(
      [{ ...sound, id: 'INVENTED', leftRules: ['typescript.no-such-rule'] }],
      KNOWN_RULE_IDS,
      NATIVE_REASONS,
    );
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('typescript.no-such-rule');
  });

  it('rejects an entry citing a reason the other implementation cannot emit', () => {
    const verdict = validateAllowlist(
      [{ ...sound, id: 'GHOST_REASON', rightReasons: ['NOT_A_REASON'] }],
      KNOWN_RULE_IDS,
      NATIVE_REASONS,
    );
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('NOT_A_REASON');
  });

  it('rejects an entry that cites no rule at all — an outcome, not a decision', () => {
    const verdict = validateAllowlist(
      [{ ...sound, id: 'BARE', leftRules: [], rightReasons: [] }],
      KNOWN_RULE_IDS,
      NATIVE_REASONS,
    );
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('describes an outcome');
  });

  it('rejects a right-stricter entry that is not marked defence-in-depth', () => {
    // right-stricter means the LEFT surface accepts what the right refuses. Since
    // the plugin is the sole authorization authority, that direction has to be
    // argued explicitly rather than waved through.
    const verdict = validateAllowlist(
      [{ ...sound, id: 'UNMARKED', direction: 'right-stricter' }],
      KNOWN_RULE_IDS,
      NATIVE_REASONS,
    );
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('defenceInDepthOnly');
  });

  it('rejects placeholder rationales and security arguments', () => {
    const verdict = validateAllowlist(
      [{ ...sound, id: 'TERSE', rationale: 'by design', securityArgument: 'safe' }],
      KNOWN_RULE_IDS,
      NATIVE_REASONS,
    );
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('rationale is too short');
    expect(verdict.problems.join('\n')).toContain('securityArgument is too short');
  });

  it('rejects duplicate class ids', () => {
    const verdict = validateAllowlist([sound, { ...sound }], KNOWN_RULE_IDS, NATIVE_REASONS);
    expect(verdict.ok).toBe(false);
    expect(verdict.problems.join('\n')).toContain('duplicate class id');
  });

  it('reports an unattributable divergence as a finding, with no "other" bucket to grow', () => {
    const reject: SideResult = { verdict: 'reject', reason: 'R', rule: 'typescript.some-rule' };
    const accept: SideResult = { verdict: 'accept', reason: 'ALLOWED', rule: null };
    const result = runDifferential({
      name: 'synthetic',
      cases: [{ input: 'x', label: 'synthetic', index: 0 }],
      left: () => reject,
      right: () => accept,
      explain: () => null, // the honest answer when nothing explains it
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(result.ok).toBe(false);
    expect(result.findings).toHaveLength(1);
    expect(String((result.findings[0] as Record<string, unknown>).why)).toContain('could not be attributed');
  });

  it('reports a divergence observed in the direction its class does NOT declare', () => {
    // The class exists and is cited, but the disagreement runs the other way. An
    // engine that matched on class id alone would excuse this, and the excuse
    // would then cover the direction that was never argued.
    const accept: SideResult = { verdict: 'accept', reason: 'ALLOWED', rule: null };
    const reject: SideResult = { verdict: 'reject', reason: 'UNSAFE_TOKEN', rule: null };
    const result = runDifferential({
      name: 'synthetic',
      cases: [{ input: 'x', label: 'synthetic', index: 0 }],
      left: () => accept,
      right: () => reject,
      // TS_ANYWHERE... is declared left-stricter; here we observe right-stricter.
      explain: () => 'TS_ANYWHERE_VS_NATIVE_FIRST_TOKEN',
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(result.ok).toBe(false);
    expect(String((result.findings[0] as Record<string, unknown>).why)).toContain('declared only for left-stricter');
  });

  it('reports a divergence attributed to a class that was never declared', () => {
    const reject: SideResult = { verdict: 'reject', reason: 'R', rule: null };
    const accept: SideResult = { verdict: 'accept', reason: 'ALLOWED', rule: null };
    const result = runDifferential({
      name: 'synthetic',
      cases: [{ input: 'x', label: 'synthetic', index: 0 }],
      left: () => reject,
      right: () => accept,
      explain: () => 'CLASS_INVENTED_AT_RUNTIME',
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(result.ok).toBe(false);
    expect(String((result.findings[0] as Record<string, unknown>).why)).toContain('not a declared asymmetry class');
  });

  it('never adjudicates a case where the two lowercasings can legitimately differ', () => {
    // UE's FString::ToLower is per-character; JavaScript's toLowerCase performs full
    // case folding. Claiming parity outside ASCII would be an overclaim, so those
    // rows must be counted as undecidable rather than scored either way.
    const result = runDifferential({
      name: 'non-ascii',
      cases: ['\u0130MPORT', 'QU\u0130T', 'stat\u00A0fps'].map((input, index) => ({
        input, label: 'non-ascii', index,
      })),
      left: typescriptSide,
      right: nativeSide,
      explain: explainCommandDivergence,
      allowlist: COMMAND_ASYMMETRY_CLASSES,
    });
    expect(result.undecidable).toBeGreaterThan(0);
    expect(result.findings).toEqual([]);
  });
});
