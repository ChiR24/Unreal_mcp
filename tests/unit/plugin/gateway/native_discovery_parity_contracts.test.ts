/// <reference types="node" />

import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { loadCanonicalRegistry, type JsonValue } from './native-discovery-model.js';
import { renderDiscovery } from './native-discovery-reference.js';

// Task 25: the native `/mcp` gateway and the TypeScript gateway must answer
// discovery identically.
//
// This does not compare the native surface to a re-implementation: the harness
// compiles the REAL native translation units (capability store, search,
// describe, canonical JSON, guidance) against the REAL generated shards and
// renders the same fixture cases. Byte-for-byte equality is the gate.

const harnessDir = resolve(process.cwd(), 'tests/harness/native-discovery');
const casesPath = resolve(harnessDir, 'cases.json');
const harnessBinary = resolve(harnessDir, 'build/native-discovery-harness');

interface FixtureCase {
  readonly name: string;
  readonly operation: string;
  readonly tool?: string;
  readonly action?: string;
  readonly param?: string;
  readonly query?: string;
  readonly domain?: string;
  readonly family?: string;
  readonly limit?: number;
  readonly offset?: number;
}

const fixtureCases = JSON.parse(readFileSync(casesPath, 'utf8')) as readonly FixtureCase[];

const runNativeHarness = (): readonly string[] => {
  execFileSync(resolve(harnessDir, 'build.sh'), { encoding: 'utf8', timeout: 240_000 });
  return execFileSync(harnessBinary, [casesPath], {
    encoding: 'utf8',
    maxBuffer: 64 * 1024 * 1024,
    timeout: 120_000,
  })
    .split('\n')
    .filter((line) => line.length > 0);
};

const referenceLines = (): readonly string[] => fixtureCases.map((entry) => renderDiscovery(entry));

/** Every number the native surface must re-serialize, i.e. excluding `examples`. */
const walkSerializedNumbers = (value: JsonValue, path: string, out: string[]): void => {
  if (typeof value === 'number') {
    if (!Number.isInteger(value)) out.push(`${path}=${value}`);
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) => walkSerializedNumbers(item, `${path}/${index}`, out));
    return;
  }
  if (value !== null && typeof value === 'object') {
    for (const [key, child] of Object.entries(value)) {
      if (key === 'examples') continue;
      walkSerializedNumbers(child, `${path}/${key}`, out);
    }
  }
};

const walkCaseCollidingKeys = (value: JsonValue, path: string, out: string[]): void => {
  if (Array.isArray(value)) {
    value.forEach((item, index) => walkCaseCollidingKeys(item, `${path}/${index}`, out));
    return;
  }
  if (value === null || typeof value !== 'object') return;
  const seen = new Map<string, string>();
  for (const [key, child] of Object.entries(value)) {
    const folded = key.toLowerCase();
    const previous = seen.get(folded);
    if (previous !== undefined) out.push(`${path}: '${previous}' vs '${key}'`);
    else seen.set(folded, key);
    walkCaseCollidingKeys(child, `${path}/${key}`, out);
  }
};

describe('Task 25: native and TypeScript discovery are byte-identical', () => {
  // The two harness-spawning cases below compile and run the native harness
  // through build.sh/corrupt-probe.sh; Windows cannot execute the shell
  // harness, so those cases run on POSIX (CI included). The pure-TypeScript
  // cases above them still run everywhere.
  it.runIf(process.platform !== 'win32')('renders every fixture case identically on both surfaces', () => {
    const native = runNativeHarness();
    const reference = referenceLines();
    expect(native.length).toBe(fixtureCases.length);
    for (let index = 0; index < reference.length; index += 1) {
      expect(native[index], `case '${fixtureCases[index]?.name}' diverged`).toBe(reference[index]);
    }
  }, 300_000);

  it('covers filters, ranking, paging, budgets, guidance and describe levels', () => {
    const names = new Set(fixtureCases.map((entry) => entry.name));
    for (const required of [
      'search-empty-default',
      'search-id-exact',
      'search-domain-filter',
      'search-unknown-domain',
      'search-byte-budget',
      'search-offset-past-end',
      'describe-tool-summary',
      'describe-capability-read',
      'describe-capability-version-gated',
      'describe-param',
      'describe-unknown-action',
    ]) {
      expect(names, `fixture case '${required}' is missing`).toContain(required);
    }
  });

  it('loads every generated record and exercises the byte budget', () => {
    const parsed = referenceLines().map((line) => JSON.parse(line) as Record<string, unknown>);
    const registry = loadCanonicalRegistry();

    const fullCatalog = parsed.find((entry) => entry.operation === 'search' && entry.total === registry.recordCount);
    expect(fullCatalog, 'no search case observed all generated records').toBeDefined();
    expect(fullCatalog?.catalogRevision).toBe(registry.catalogRevision);

    const truncated = parsed.filter((entry) => entry.truncated === true);
    expect(truncated.length, 'byte budget never fired, so the bound is untested').toBeGreaterThan(0);

    const contract = parsed.find((entry) => entry.capability === 'animation_physics.get_animation_info');
    expect(contract?.hashes).toMatchObject({ algorithm: 'sha256' });
  });

  it('keeps the canonical corpus inside the cross-language serialization rules', () => {
    const registry = loadCanonicalRegistry();

    const floats: string[] = [];
    walkSerializedNumbers(registry.records as unknown as JsonValue, '', floats);
    expect(floats, 'a non-integer number outside `examples` would break byte-parity').toEqual([]);

    // UE's TMap<FString,...> hashes and compares case-insensitively, so two
    // sibling JSON keys differing only by case COLLAPSE into one when the native
    // surface parses them. Exactly one record in the canonical corpus does this;
    // it is pinned below by native_discovery_known_divergence_contracts so a new
    // occurrence fails here instead of silently dropping a parameter.
    const knownCaseCollisions = [
      "/956/schemas/input/properties: 'subLevelPath' vs 'sublevelPath'",
    ];
    const collisions: string[] = [];
    walkCaseCollidingKeys(registry.records as unknown as JsonValue, '', collisions);
    expect(collisions, "UE's TMap folds case, so sibling keys differing only by case collide").toEqual(
      knownCaseCollisions,
    );

    const badIds = registry.records
      .map((record) => record.id)
      .filter((id) => id !== id.toLowerCase() || /[^\x20-\x7e]/u.test(id));
    expect(badIds, 'ordinal and case-insensitive ordering only agree for lowercase ASCII ids').toEqual([]);
  });

  it.runIf(process.platform !== 'win32')('refuses discovery when a generated shard is corrupted', () => {
    const output = execFileSync(resolve(harnessDir, 'corrupt-probe.sh'), ['truncate-json'], {
      encoding: 'utf8',
      timeout: 300_000,
    });
    expect(output).toContain('REFUSED');
    expect(output).not.toContain('SERVED_ANYWAY');
  }, 300_000);
});
