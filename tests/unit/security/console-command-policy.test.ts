import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { CommandValidator } from '../../../src/utils/commands/command-validator.js';
import {
  CONSOLE_COMMAND_POLICY_CASES,
  expectedConsoleCommandPolicyOutcomes,
} from '../../../src/utils/commands/console-command-policy-fixture.js';
import {
  evaluateCurrentConsoleCommandPolicy,
  evaluateIntendedConsoleCommandPolicy,
} from '../../../src/utils/commands/console-command-policy-model.js';
import {
  CONSOLE_COMMAND_POLICY_GENERATED_TS_PATH,
  CONSOLE_COMMAND_POLICY_GENERATED_NATIVE_PATH,
  GENERATED_POLICY_HEADER,
  applyGeneratedConsoleCommandPolicy,
  serializeConsoleCommandPolicyGenerated,
} from '../../../src/utils/commands/console-command-policy-generated.js';
import { runConsoleCommandPolicyGenerator } from '../../../scripts/generate-console-command-policy.js';

function runtimeTypescriptBlocked(command: string): boolean {
  try {
    CommandValidator.validate(command);
    return false;
  } catch (error: unknown) {
    if (error instanceof Error) return true;
    throw error;
  }
}

describe('Task 22 - generated console-command policy enforcement', () => {
  it('zero runtime delta: generated artifacts reproduce every current case on both surfaces', () => {
    const cases = CONSOLE_COMMAND_POLICY_CASES;
    const mismatches = cases.flatMap((testCase) => {
      const expected = expectedConsoleCommandPolicyOutcomes(testCase.bucket);
      const modelTs = evaluateCurrentConsoleCommandPolicy(testCase.command, 'typescript');
      const modelNat = evaluateCurrentConsoleCommandPolicy(testCase.command, 'native');
      const genTs = applyGeneratedConsoleCommandPolicy(testCase.command, 'typescript');
      const genNat = applyGeneratedConsoleCommandPolicy(testCase.command, 'native');
      const runtimeTs = runtimeTypescriptBlocked(testCase.command);
      return modelTs.blocked === expected.typescriptBlocked
        && modelNat.blocked === expected.nativeBlocked
        && genTs === expected.typescriptBlocked
        && genNat === expected.nativeBlocked
        && runtimeTs === expected.typescriptBlocked
        ? []
        : [`${testCase.id}: model=${modelTs.blocked}/${modelNat.blocked} gen=${genTs}/${genNat} runtime-ts=${runtimeTs}`];
    });
    expect(mismatches).toEqual([]);
  });

  it('generated artifacts are owned (not hand-edited) and present', () => {
    const tsPath = resolve(process.cwd(), CONSOLE_COMMAND_POLICY_GENERATED_TS_PATH);
    const nativePath = resolve(process.cwd(), CONSOLE_COMMAND_POLICY_GENERATED_NATIVE_PATH);
    const tsExists = existsSync(tsPath);
    const nativeExists = existsSync(nativePath);
    expect(tsExists).toBe(true);
    expect(nativeExists).toBe(true);
    if (tsExists) {
      const tsSource = readFileSync(tsPath, 'utf8');
      expect(tsSource).toContain(GENERATED_POLICY_HEADER);
    }
    if (nativeExists) {
      const nativeSource = readFileSync(nativePath, 'utf8');
      expect(nativeSource).toContain('GENERATED');
    }
  });

  it('no duplicated handwritten lists: validator is wired to the generated policy', () => {
    const validatorPath = resolve(process.cwd(), 'src/utils/commands/command-validator.ts');
    const validatorSource = readFileSync(validatorPath, 'utf8');
    expect(validatorSource).toContain('console-command-policy-generated');
  });

  it('generated drift fails CI: regeneration is byte-stable against committed artifacts', () => {
    const { tsContent, nativeContent } = runConsoleCommandPolicyGenerator();
    const tsPath = resolve(process.cwd(), CONSOLE_COMMAND_POLICY_GENERATED_TS_PATH);
    const nativePath = resolve(process.cwd(), CONSOLE_COMMAND_POLICY_GENERATED_NATIVE_PATH);
    expect(readFileSync(tsPath, 'utf8')).toBe(tsContent);
    expect(readFileSync(nativePath, 'utf8')).toBe(nativeContent);
  });

  it('generated serialization is deterministic across repeated runs', () => {
    const first = serializeConsoleCommandPolicyGenerated();
    const second = serializeConsoleCommandPolicyGenerated();
    expect(second).toBe(first);
  });

  it('existing safe commands remain allowed under the generated policy', () => {
    const safe = ['stat fps', 'viewmode lit', 'help', 'show', 'quitter', 'rmdebug', 'stat unit', 'obj savepackage'];
    const outcomes = safe.map(
      (command) =>
        applyGeneratedConsoleCommandPolicy(command, 'typescript') &&
        applyGeneratedConsoleCommandPolicy(command, 'native'),
    );
    expect(outcomes.every((blocked) => blocked === false)).toBe(true);
  });

  it('intended fail-closed union preserves every blocked reason code', () => {
    const divergent = CONSOLE_COMMAND_POLICY_CASES.filter(
      (testCase) => testCase.bucket !== 'equivalent-block' && testCase.bucket !== 'equivalent-allow',
    );
    const decisions = divergent.map((testCase) =>
      evaluateIntendedConsoleCommandPolicy(testCase.command),
    );
    expect(decisions.every((decision) => decision.blocked)).toBe(true);
  });
});
