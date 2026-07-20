/// <reference types="node" />

import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { renderDiscovery } from './native-discovery-reference.js';

// Task 25 KNOWN DIVERGENCE — data defect, not a discovery-logic defect.
//
// `manage_level.add_sublevel` declares two sibling input-schema properties that
// differ only by case: `subLevelPath` (canonical, required) and `sublevelPath`
// ("Alias of subLevelPath resolved by the manage_level argument normalizer").
//
// UE's TMap<FString, ...> hashes and compares FString case-insensitively, so
// FJsonObject collapses the pair and the native surface can only ever advertise
// one of them. No native discovery implementation can fix this while the record
// declares both keys: the loss happens inside the engine container.
//
// This is PRE-EXISTING (the previous manifest-backed describe collapsed them the
// same way) and is deliberately NOT worked around here, because the fix belongs
// to the record data: a case-variant alias belongs in the Task-20 migration /
// alias data, not as a duplicate declared property under additionalProperties:
// false. Removing it also changes the TypeScript execute contract, which is
// outside this task's ownership.
//
// The exact current behavior is pinned so the divergence stays visible and any
// change to it is deliberate.

const harnessDir = resolve(process.cwd(), 'tests/harness/native-discovery');
const casesPath = resolve(harnessDir, 'known-divergence-cases.json');
const harnessBinary = resolve(harnessDir, 'build/native-discovery-harness');

interface ParameterView { readonly name: string }
interface DescribeResult { readonly parameters: readonly ParameterView[] }

const parameterNames = (line: string): readonly string[] =>
  (JSON.parse(line) as DescribeResult).parameters.map((entry) => entry.name);

describe('Task 25: known native/TypeScript divergence from case-colliding schema keys', () => {
  it('drops the case-variant alias natively while TypeScript keeps both', () => {
    execFileSync(resolve(harnessDir, 'build.sh'), { encoding: 'utf8', timeout: 240_000 });
    const nativeLine = execFileSync(harnessBinary, [casesPath], {
      encoding: 'utf8',
      maxBuffer: 16 * 1024 * 1024,
      timeout: 120_000,
    }).trim();

    const fixture = JSON.parse(readFileSync(casesPath, 'utf8')) as readonly Record<string, unknown>[];
    const referenceLine = renderDiscovery(fixture[0] as never);

    expect(parameterNames(referenceLine)).toContain('subLevelPath');
    expect(parameterNames(referenceLine)).toContain('sublevelPath');

    expect(parameterNames(nativeLine)).toContain('subLevelPath');
    expect(parameterNames(nativeLine)).not.toContain('sublevelPath');
  }, 300_000);
});
