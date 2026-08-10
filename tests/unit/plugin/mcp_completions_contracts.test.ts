import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { countPureLines } from './plugin-contract-fixtures.js';

const root = process.cwd();
const nativeRoot = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP');
const completionsRoot = resolve(root, 'src/server/mcp-primitives/completions');

const header = readFileSync(resolve(nativeRoot, 'Primitives/McpCompletionProvider.h'), 'utf8');
const cpp = readFileSync(resolve(nativeRoot, 'Primitives/McpCompletionProvider.cpp'), 'utf8');
const nativeText = `${header}\n${cpp}`;

const tsTypes = readFileSync(resolve(completionsRoot, 'completion-types.ts'), 'utf8');
// The path/secret vocabulary these contracts compare against the native side
// now lives in one shared policy module, so it is appended to the TS text
// under test: the parity assertion is about what the TS SURFACE enforces,
// not about which file happens to hold the literal.
const tsPathPolicy = readFileSync(resolve(root, 'src/utils/paths/content-path-policy.ts'), 'utf8');
const tsSlots = readFileSync(resolve(completionsRoot, 'completion-slots.ts'), 'utf8') + tsPathPolicy;
const tsRanking = readFileSync(resolve(completionsRoot, 'completion-ranking.ts'), 'utf8');
const tsSources = readFileSync(resolve(completionsRoot, 'completion-sources.ts'), 'utf8');
const tsText = `${tsTypes}\n${tsSlots}\n${tsRanking}\n${tsSources}`;

interface ParityFixture {
  readonly budgets: { readonly maxItems: number; readonly maxBytes: number; readonly maxPrefixLength: number };
  readonly guidanceCodes: readonly string[];
  readonly candidateKinds: readonly string[];
  readonly slots: readonly { readonly refId: string; readonly argumentName: string }[];
  readonly enumSets: Readonly<Record<string, readonly string[]>>;
  readonly secretFragments: readonly string[];
  readonly destructiveFragments: readonly string[];
}

const fixture = JSON.parse(
  readFileSync(resolve(completionsRoot, 'completion-parity.fixture.json'), 'utf8'),
) as ParityFixture;

describe('mcp completions native source contracts', () => {
  it('mirrors the bounded budgets on both surfaces', () => {
    for (const budget of [fixture.budgets.maxItems, fixture.budgets.maxBytes, fixture.budgets.maxPrefixLength]) {
      expect(header).toContain(String(budget));
      expect(tsTypes).toContain(String(budget));
    }
  });

  it('mirrors the typed guidance codes on both surfaces', () => {
    for (const code of fixture.guidanceCodes) {
      expect(header, `native must declare ${code}`).toContain(code);
      expect(tsTypes, `ts must declare ${code}`).toContain(code);
    }
  });

  it('mirrors the completable slot ids and argument names on both surfaces', () => {
    for (const slot of fixture.slots) {
      expect(nativeText, `native must reference ${slot.refId}`).toContain(slot.refId);
      expect(nativeText, `native must reference ${slot.argumentName}`).toContain(slot.argumentName);
      expect(tsText, `ts must reference ${slot.refId}`).toContain(slot.refId);
      expect(tsText, `ts must reference ${slot.argumentName}`).toContain(slot.argumentName);
    }
  });

  it('mirrors the candidate kinds on both surfaces', () => {
    for (const kind of fixture.candidateKinds) {
      expect(nativeText, `native kind ${kind}`).toContain(`"${kind}"`);
      expect(tsText, `ts kind ${kind}`).toContain(`'${kind}'`);
    }
  });

  it('mirrors every bounded enum value set on both surfaces', () => {
    for (const values of Object.values(fixture.enumSets)) {
      for (const value of values) {
        expect(nativeText, `native enum value ${value}`).toContain(`"${value}"`);
        expect(tsText, `ts enum value ${value}`).toContain(`'${value}'`);
      }
    }
  });

  it('mirrors the secret and destructive safety fragments on both surfaces', () => {
    for (const fragment of [...fixture.secretFragments, ...fixture.destructiveFragments]) {
      expect(cpp, `native fragment ${fragment}`).toContain(fragment);
      expect(tsSlots, `ts fragment ${fragment}`).toContain(fragment);
    }
  });

  it('carries the same deterministic ranking ladder on both surfaces', () => {
    for (const symbol of ['StartsWith', 'IsSubsequence', 'WithinOneEdit', 'McpApplyCompletionBudget']) {
      expect(cpp).toContain(symbol);
    }
    for (const symbol of ['startsWith', 'isSubsequence', 'withinOneEdit', 'applyBudget']) {
      expect(tsRanking).toContain(symbol);
    }
  });

  it('never emits a host path or reads the project-path env in native completion metadata', () => {
    for (const forbidden of ['/home/', '/Users/', 'C:\\', '.uproject', 'UE_PROJECT_PATH', 'UPackage::SavePackage']) {
      expect(nativeText).not.toContain(forbidden);
    }
  });

  it('keeps each native completion file within the 250 pure-line ceiling', () => {
    expect(countPureLines(header)).toBeLessThanOrEqual(250);
    expect(countPureLines(cpp)).toBeLessThanOrEqual(250);
  });
});
