// Plan Todo 17 (BB-011) - a broad CDO/property export must be bounded and must
// say so, while the targeted propertyNames lookup stays exact and unbounded.
//
// Written after the fix landed, so non-vacuity is proven by mutation. Assertions
// run over comment-stripped text and are anchored on the statement that BINDS a
// value, never on a bare substring: an earlier todo proved that a dead literal,
// a widened `* 100`, or prose in a comment can satisfy a naive `toContain`.
//
// The BB-062 screenshot half of this todo is recorded separately; the resolution
// work landed from another lane and the base64 budget is a cross-transport
// decision, so no screenshot case is asserted here yet.

import { describe, expect, it } from 'vitest';

import { privateSource } from './sequence_contract_test_utils.js';

/** Blank comments so no assertion can be satisfied by prose. */
function code(source: string): string {
  return source
    .replace(/\/\*[\s\S]*?\*\//gu, ' ')
    .replace(/\/\/[^\n]*/gu, ' ');
}

const header = (): string =>
  code(privateSource('Foundation', 'Reflection', 'McpPropertyReflection.h'));
const definition = (): string =>
  code(privateSource('Foundation', 'Reflection', 'McpPropertyReflectionObject.cpp'));
const inspection = (): string =>
  code(privateSource('Domains', 'Property', 'McpAutomationBridge_PropertyHandlersCdoInspection.cpp'));
const components = (): string =>
  code(privateSource('Domains', 'Property', 'McpAutomationBridge_PropertyHandlersCdoComponents.cpp'));

const CONSUMERS: readonly (readonly [string, () => string, number])[] = [
  ['CdoInspection', inspection, 2],
  ['CdoComponents', components, 1]
];

describe('todo17 BB-011: the broad export is bounded at its declaration', () => {
  it('declares the cap as a terminated constant, not a loose number', () => {
    // Anchored through the semicolon so `= 2000` cannot satisfy `= 200`.
    expect(header()).toMatch(/static constexpr int32 McpMaxBoundedExportProperties = 200;/u);
  });

  it('declares the bounded variant defaulted to that constant', () => {
    expect(header()).toMatch(
      /ExportObjectToJsonBounded\(UObject\* Object, bool bIncludeTransient = false, int32 MaxProperties = McpMaxBoundedExportProperties\);/u
    );
  });

  it('keeps the unbounded and targeted exports available for other callers', () => {
    const source = header();

    expect(source).toMatch(/ExportObjectToJson\(UObject\* Object, bool bIncludeTransient = false\);/u);
    expect(source).toMatch(/ExportPropertiesToJson\(UObject\* Object, const TArray<FName>& PropertyNames\);/u);
  });
});

describe('todo17 BB-011: the bounded body reports honestly', () => {
  it('clamps the cap rather than trusting the caller', () => {
    expect(definition()).toMatch(/const int32 Cap = FMath::Max\(0, MaxProperties\);/u);
  });

  it('claims truncation ONLY where the cap is what stopped the walk', () => {
    const source = definition();
    const flags = [...source.matchAll(/bTruncated = true/gu)];

    expect(flags).toHaveLength(1);
    // Bound to the cap branch: a truncation flag set anywhere else would report
    // an export that merely failed as one that was deliberately withheld.
    for (const match of flags) {
      const before = source.slice(Math.max(0, (match.index ?? 0) - 60), match.index);
      expect(before, 'truncation must be set inside the cap branch').toMatch(/if \(Emitted >= Cap\)\s*\{\s*$/u);
    }
  });

  it('counts every eligible property but emits only what it wrote', () => {
    const source = definition();

    // Total advances before the cap test, so propertyCount stays the true total.
    expect(source).toMatch(/\+\+Total;[\s\S]{0,120}if \(Emitted >= Cap\)/u);
    // Emitted advances only inside the valid-value branch, so the count cannot
    // drift above the number of fields actually present in the payload.
    expect(source).toMatch(/Result->SetField\(Property->GetName\(\), Value\);\s*\+\+Emitted;/u);
  });

  it('emits the three bookkeeping keys under a UPROPERTY-impossible prefix', () => {
    const source = definition();

    expect(source).toContain('Result->SetNumberField(TEXT("$mcpPropertyCount"), Total);');
    expect(source).toContain('Result->SetNumberField(TEXT("$mcpMaxProperties"), Cap);');
    expect(source).toContain('Result->SetBoolField(TEXT("$mcpTruncated"), bTruncated);');
  });
});

describe('todo17 BB-011: every broad call site is bounded, every targeted one is not', () => {
  it.each(CONSUMERS)('%s routes its broad export through the bounded variant', (name, read, expected) => {
    const source = read();
    const unbounded = source.match(/ExportObjectToJson\(/gu) ?? [];
    const bounded = source.match(/ExportObjectToJsonBounded\(/gu) ?? [];

    expect(unbounded, `${name}: no broad export may stay unbounded`).toHaveLength(0);
    expect(bounded, `${name}: bounded call sites`).toHaveLength(expected);
  });

  it.each(CONSUMERS)('%s takes the default cap at every call site', (name, read) => {
    const source = read();
    const calls = [...source.matchAll(/ExportObjectToJsonBounded\(([^)]*)\)/gu)];

    expect(calls.length).toBeGreaterThan(0);
    // A third argument keeps the per-file counts above identical while defeating
    // the bound outright, so the arguments themselves have to be pinned.
    for (const match of calls) {
      const args = (match[1] ?? '').split(',').map((arg) => arg.trim());

      expect(args, `${name}: ${match[0]} must not override MaxProperties`).toHaveLength(2);
      expect(args[1], `${name}: ${match[0]} must not walk transient properties`).toBe('false');
    }
  });

  it.each(CONSUMERS)('%s keeps the exact targeted lookup untouched', (name, read) => {
    const source = read();

    expect(
      source.match(/ExportPropertiesToJson\(/gu) ?? [],
      `${name}: the propertyNames path must stay exact and uncapped`
    ).not.toHaveLength(0);
  });

  it('the detailed branch is the bounded one, and the filtered branch is not', () => {
    const source = inspection();

    // The filter branch runs first and must still call the targeted export; the
    // else-branch below it is the broad walk that needed the cap.
    expect(source).toMatch(
      /PropertyNameFilter\.Num\(\) > 0[\s\S]{0,200}ExportPropertiesToJson\(CDO, PropertyNameFilter\)/u
    );
    expect(source).toMatch(/else if \(bDetailed\)[\s\S]{0,200}ExportObjectToJsonBounded\(CDO, false\)/u);
  });
});
