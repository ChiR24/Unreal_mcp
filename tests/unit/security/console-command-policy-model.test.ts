import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { CommandValidator } from '../../../src/utils/commands/command-validator.js';
import {
  CONSOLE_COMMAND_POLICY_CASES,
  expectedConsoleCommandPolicyOutcomes,
  formatConsoleCommandPolicyReport,
  serializeConsoleCommandPolicyFixture,
  tallyConsoleCommandPolicyCases,
} from '../../../src/utils/commands/console-command-policy-fixture.js';
import {
  CONSOLE_COMMAND_POLICY_GENERATOR_REQUIREMENTS,
  CONSOLE_COMMAND_POLICY_NORMALIZATION,
  evaluateConsoleCommandPolicyInput,
  evaluateCurrentConsoleCommandPolicy,
  evaluateIntendedConsoleCommandPolicy,
  serializeConsoleCommandPolicyModel,
} from '../../../src/utils/commands/console-command-policy-model.js';
import {
  CONSOLE_COMMAND_POLICY_BLOCK_REASONS,
  CONSOLE_COMMAND_POLICY_RULES,
} from '../../../src/utils/commands/console-command-policy-rules.js';

function blockedPair(command: string): readonly [boolean, boolean] {
  return [
    evaluateCurrentConsoleCommandPolicy(command, 'typescript').blocked,
    evaluateCurrentConsoleCommandPolicy(command, 'native').blocked,
  ];
}

function runtimeTypescriptBlocked(command: string): boolean {
  try {
    CommandValidator.validate(command);
    return false;
  } catch (error: unknown) {
    if (error instanceof Error) return true;
    throw error;
  }
}

describe('console-command policy model', () => {
  it('provides typed policy data when Task 6 is implemented', () => {
    // Given: Task 6 requires a behavior-neutral model outside the runtime validator.
    const validatorPath = resolve(process.cwd(), 'src/utils/commands/command-validator.ts');

    // When: the existing runtime validator source is inspected after model creation.
    const validatorSource = readFileSync(validatorPath, 'utf8');

    // Then: the new model remains unwired and cannot change current filtering behavior.
    expect(validatorSource).not.toContain('console-command-policy');
  });

  it('reproduces every current surface outcome in the locked corpus', () => {
    // Given: the reviewed cross-transport baseline cases and their typed buckets.
    const cases = CONSOLE_COMMAND_POLICY_CASES;

    // When: both current-policy models evaluate every command.
    const mismatches = cases.flatMap((testCase) => {
      const expected = expectedConsoleCommandPolicyOutcomes(testCase.bucket);
      const actual = blockedPair(testCase.command);
      const runtimeTypescript = runtimeTypescriptBlocked(testCase.command);
      return actual[0] === expected.typescriptBlocked
        && actual[1] === expected.nativeBlocked
        && runtimeTypescript === expected.typescriptBlocked
        ? []
        : [`${testCase.id}:${actual[0]}/${actual[1]}/runtime-ts=${runtimeTypescript}`];
    });

    // Then: the executable model has zero drift from the locked baseline.
    expect(mismatches).toEqual([]);
  });

  it('locks the reviewed bucket counts and intended union count', () => {
    // Given: all 73 baseline cases, including six shared safe commands.
    const cases = CONSOLE_COMMAND_POLICY_CASES;

    // When: the deterministic tally is computed.
    const tally = tallyConsoleCommandPolicyCases(cases);

    // Then: all reviewed delta buckets and the fail-closed union are exact.
    expect(tally).toEqual({
      totalCases: 73,
      typescriptOnly: 13,
      nativeOnly: 5,
      typescriptOverBlock: 3,
      equivalentBlocks: 46,
      equivalentAllows: 6,
      intendedUnionBlocks: 67,
    });
  });

  it('uses the deterministic fail-closed union for intended decisions', () => {
    // Given: cases that one surface blocks and the other currently allows.
    const divergentCases = CONSOLE_COMMAND_POLICY_CASES.filter(
      (testCase) => testCase.bucket !== 'equivalent-block' && testCase.bucket !== 'equivalent-allow',
    );

    // When: the intended policy evaluates the divergent commands.
    const decisions = divergentCases.map((testCase) =>
      evaluateIntendedConsoleCommandPolicy(testCase.command),
    );

    // Then: every one-sided block remains blocked and names its enforcing surface.
    expect(decisions.every((decision) => decision.blocked)).toBe(true);
    expect(decisions.every((decision) => decision.sourceSurfaces.length >= 1)).toBe(true);
  });

  it('fails closed on malformed policy input', () => {
    // Given: values that are not console-command strings.
    const malformedInputs: readonly unknown[] = [null, undefined, 42, {}, []];

    // When: both surface models parse the untrusted values.
    const decisions = malformedInputs.flatMap((input) => [
      evaluateConsoleCommandPolicyInput(input, 'typescript'),
      evaluateConsoleCommandPolicyInput(input, 'native'),
    ]);

    // Then: every malformed value is blocked with a stable reason code.
    expect(decisions.every((decision) => decision.blocked)).toBe(true);
    expect(decisions.every((decision) => decision.reasonCode === 'MALFORMED_INPUT')).toBe(true);
  });

  it('preserves the verified internal-whitespace asymmetry', () => {
    // Given: literal, repeated, tab, and Unicode whitespace between import tokens.
    const variants = ['import os', 'import  os', 'import\tos', 'import\u00a0os'];

    // When: both current-policy models evaluate each whitespace form.
    const outcomes = variants.map(blockedPair);

    // Then: native matches only the literal-space form while TypeScript matches all forms.
    expect(outcomes).toEqual([
      [true, true],
      [true, false],
      [true, false],
      [true, false],
    ]);
  });

  it('blocks every unsafe separator on both surfaces', () => {
    // Given: each current command separator and a safe command on both sides.
    const separators = ['\n', '\r', '&&', '||', ';', '|', '`'];

    // When: the separators are embedded in commands.
    const outcomes = separators.map((separator) => blockedPair(`stat${separator}fps`));

    // Then: both surfaces reject every separator variant.
    expect(outcomes.every(([typescript, native]) => typescript && native)).toBe(true);
  });

  it('normalizes first-token casing before rule evaluation', () => {
    // Given: mixed-case and edge-whitespace forms of a blocked first token.
    const variants = ['QUIT', 'Quit', 'qUiT', '  QUIT\t'];

    // When: both current-policy models evaluate the variants.
    const outcomes = variants.map(blockedPair);

    // Then: casing and surrounding whitespace cannot bypass either surface.
    expect(outcomes.every(([typescript, native]) => typescript && native)).toBe(true);
  });

  it('preserves Unicode lookalikes without compatibility folding', () => {
    // Given: visually similar tokens containing full-width, zero-width, and Cyrillic code points.
    const lookalikes = ['ｑｕｉｔ', 'qu\u200dit', 'pу'];

    // When: both current-policy models evaluate the lookalikes.
    const outcomes = lookalikes.map(blockedPair);

    // Then: the model records current code-point semantics rather than inventing NFKC behavior.
    expect(outcomes).toEqual([
      [false, false],
      [false, false],
      [false, false],
    ]);
  });

  it('does not turn prefix collisions into exact command matches', () => {
    // Given: safe tokens that only begin with blocked command names.
    const collisions = ['quitter', 'rmdebug', 'pythonista', 'debugbreaker'];

    // When: both current-policy models evaluate each prefix collision.
    const outcomes = collisions.map(blockedPair);

    // Then: all remain allowed under the recorded current and intended union policies.
    expect(outcomes.every(([typescript, native]) => !typescript && !native)).toBe(true);
    expect(collisions.every((command) => !evaluateIntendedConsoleCommandPolicy(command).blocked)).toBe(true);
  });

  it('exposes stable normalization, applicability, and reason-code data', () => {
    // Given: the typed rules and generator contract.
    const ruleIds = CONSOLE_COMMAND_POLICY_RULES.map((rule) => rule.id);

    // When: rule identity and model metadata are inspected.
    const uniqueRuleIds = new Set(ruleIds);

    // Then: generation inputs are explicit, unique, and deferred from runtime wiring.
    expect(uniqueRuleIds.size).toBe(ruleIds.length);
    expect(CONSOLE_COMMAND_POLICY_RULES.every((rule) =>
      CONSOLE_COMMAND_POLICY_BLOCK_REASONS.includes(rule.reasonCode))).toBe(true);
    expect(CONSOLE_COMMAND_POLICY_NORMALIZATION.intended.strategy).toBe('fail-closed-union');
    expect(CONSOLE_COMMAND_POLICY_GENERATOR_REQUIREMENTS.runtimeWiring).toBe('deferred-to-task-22');
  });

  it('serializes the model and fixture deterministically', () => {
    // Given: immutable policy rules and baseline fixture data.
    const firstModel = serializeConsoleCommandPolicyModel();
    const firstFixture = serializeConsoleCommandPolicyFixture();

    // When: both serializations are repeated.
    const secondModel = serializeConsoleCommandPolicyModel();
    const secondFixture = serializeConsoleCommandPolicyFixture();

    // Then: byte-for-byte output is stable.
    expect(secondModel).toBe(firstModel);
    expect(secondFixture).toBe(firstFixture);
  });

  it('prints the exact policy-report channel', () => {
    // Given: the locked baseline tally.
    const expected = [
      'TS-only blocks (13)',
      'Native-only blocks (5)',
      'TS over-blocks (3)',
      'Equivalent blocks (46)',
      'Intended fail-closed union blocks (67)',
      'Corpus cases (73)',
      'Equivalent allows (6)',
    ].join('\n');

    // When: the literal report formatter is invoked.
    const report = formatConsoleCommandPolicyReport();

    // Then: the four reviewed buckets and intended union count are exact.
    expect(report).toBe(expected);
  });
});
