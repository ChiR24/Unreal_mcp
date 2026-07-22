// Task 33: locks the TypeScript completion modules to the neutral normalized
// parity fixture (completion-parity.fixture.json). The native mirror is checked
// against the same fixture by tests/unit/plugin/mcp_completions_contracts.test.ts,
// so both surfaces normalize to one shared contract.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

import { COMPLETION_SLOTS } from './completion-slots.js';
import { createStaticCompletionSource } from './completion-sources.js';
import {
  CANDIDATE_KINDS,
  COMPLETION_GUIDANCE_CODES,
  MAX_COMPLETION_BYTES,
  MAX_COMPLETION_ITEMS,
  MAX_PREFIX_LENGTH,
  SLOT_KINDS,
  type CompletionSlot,
} from './completion-types.js';

interface ParityFixture {
  readonly budgets: { readonly maxItems: number; readonly maxBytes: number; readonly maxPrefixLength: number };
  readonly guidanceCodes: readonly string[];
  readonly candidateKinds: readonly string[];
  readonly slotKinds: readonly string[];
  readonly slots: readonly CompletionSlot[];
  readonly enumSets: Readonly<Record<string, readonly string[]>>;
  readonly secretFragments: readonly string[];
  readonly destructiveFragments: readonly string[];
}

const fixturePath = fileURLToPath(new URL('./completion-parity.fixture.json', import.meta.url));
const fixture = JSON.parse(readFileSync(fixturePath, 'utf8')) as ParityFixture;
const source = createStaticCompletionSource();

describe('TS completion modules match the normalized parity fixture', () => {
  it('mirrors the bounded budgets', () => {
    expect(MAX_COMPLETION_ITEMS).toBe(fixture.budgets.maxItems);
    expect(MAX_COMPLETION_BYTES).toBe(fixture.budgets.maxBytes);
    expect(MAX_PREFIX_LENGTH).toBe(fixture.budgets.maxPrefixLength);
  });

  it('mirrors the guidance codes, candidate kinds, and slot kinds', () => {
    expect([...Object.values(COMPLETION_GUIDANCE_CODES)].sort()).toEqual([...fixture.guidanceCodes].sort());
    expect([...CANDIDATE_KINDS]).toEqual([...fixture.candidateKinds]);
    expect([...SLOT_KINDS]).toEqual([...fixture.slotKinds]);
  });

  it('mirrors the completable slot table exactly', () => {
    expect(COMPLETION_SLOTS).toEqual(fixture.slots);
  });

  it('reproduces every enum value set from the static source', () => {
    const enumSlots = COMPLETION_SLOTS.filter((slot) => slot.kind === 'enum');
    expect(enumSlots.length).toBe(Object.keys(fixture.enumSets).length);
    for (const slot of enumSlots) {
      const expected = fixture.enumSets[slot.argumentName];
      expect(expected, `fixture has an enum set for ${slot.argumentName}`).toBeDefined();
      expect(source.enumCandidates(slot).map((candidate) => candidate.value)).toEqual(expected);
    }
  });
});
