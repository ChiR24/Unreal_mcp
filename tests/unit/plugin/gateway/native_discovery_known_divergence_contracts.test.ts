/// <reference types="node" />

import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { renderDiscovery } from './native-discovery-reference.js';

// Task 25 known divergence — CLOSED.
//
// `manage_level.add_sublevel` used to declare two sibling input-schema
// properties differing only by case: `subLevelPath` (canonical) and
// `sublevelPath` ("alias of subLevelPath"). UE's TMap<FString, ...> hashes and
// compares FString case-insensitively, so FJsonObject collapsed the pair and
// the native surface could only ever advertise one of them. The alias
// therefore worked over TypeScript stdio and answered UNDECLARED_PARAMETER
// over native `/mcp` — a parameter that existed on one transport and not the
// other, which is worse than not having it at all.
//
// No discovery implementation could fix that while the record declared both
// keys, because the loss happens inside the engine container. The earlier note
// here proposed moving the alias into the migration/alias data instead, but
// that data maps legacy {tool, action} pairs to canonical capabilities; there
// is no parameter-alias stage anywhere on the execute path, and declared
// properties are the only accepted names on either transport
// (`gateway-schema-validate.ts`, mirrored natively). So the pair was resolved
// in the record data by dropping the spelling that differed only by case.
// `levelPath` remains as a second accepted spelling and cannot collide.
//
// This case is kept — inverted — so the harness still proves the two surfaces
// agree on this record, and reintroducing a case-variant alias fails here
// rather than silently dropping a parameter natively.

const harnessDir = resolve(process.cwd(), 'tests/harness/native-discovery');
const casesPath = resolve(harnessDir, 'known-divergence-cases.json');
const harnessBinary = resolve(harnessDir, 'build/native-discovery-harness');

interface ParameterView { readonly name: string }
interface DescribeResult { readonly parameters: readonly ParameterView[] }

const parameterNames = (line: string): readonly string[] =>
  (JSON.parse(line) as DescribeResult).parameters.map((entry) => entry.name);

describe('Task 25: the case-colliding schema keys are gone from both surfaces', () => {
  // Windows cannot execute the .sh harness, so this case runs on POSIX (CI
  // included).
  it.runIf(process.platform !== 'win32')('advertises an identical parameter set on both transports', () => {
    execFileSync(resolve(harnessDir, 'build.sh'), { encoding: 'utf8', timeout: 240_000 });
    const nativeLine = execFileSync(harnessBinary, [casesPath], {
      encoding: 'utf8',
      maxBuffer: 16 * 1024 * 1024,
      timeout: 120_000,
    }).trim();

    const fixture = JSON.parse(readFileSync(casesPath, 'utf8')) as readonly Record<string, unknown>[];
    const referenceLine = renderDiscovery(fixture[0] as never);

    const reference = parameterNames(referenceLine);
    const native = parameterNames(nativeLine);

    expect(reference).toContain('subLevelPath');
    expect(reference).not.toContain('sublevelPath');
    expect(native).not.toContain('sublevelPath');

    // The whole point of the fix: nothing is lost crossing into the engine
    // container any more, so the two parameter sets are equal, not merely
    // overlapping.
    expect([...native].sort()).toEqual([...reference].sort());
  }, 300_000);
});
