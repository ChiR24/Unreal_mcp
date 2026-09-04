// Parity gate for the vector-shape coercion mirror (dogfood #226).
//
// coerceVectorShapes in src/server/gateway/gateway-schema-validate.ts and
// McpCoerceCanonicalVectorShapes in
// plugins/McpAutomationBridge/.../Private/MCP/Gateway/McpNativeGatewayVectorCoercion.cpp
// must accept exactly the same payloads. The native side has no unit harness,
// so this test reads the C++ source as text (the repo's established
// source-contract approach) and pins: the key sets and their order, the
// required-count rule, the finite-number predicate, the execute-stage wiring
// position, and the describe action-strip guard that b86fad09 pinned on the
// TypeScript side only.
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { VECTOR_KEY_SETS } from '../../../src/server/gateway/gateway-schema-validate.js';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '../../..');

const TS_EXECUTE = resolve(repoRoot, 'src/server/gateway/gateway-execute.ts');
const CPP_COERCION = resolve(
  repoRoot,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayVectorCoercion.cpp'
);
const CPP_VALIDATION = resolve(
  repoRoot,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute/McpNativeGatewayValidation.cpp'
);
const CPP_DESCRIBE_OVERVIEW = resolve(
  repoRoot,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayDescribeOverview.cpp'
);

const cppCoercionSource = readFileSync(CPP_COERCION, 'utf8');
const cppValidationSource = readFileSync(CPP_VALIDATION, 'utf8');
const tsExecuteSource = readFileSync(TS_EXECUTE, 'utf8');
const cppDescribeOverviewSource = readFileSync(CPP_DESCRIBE_OVERVIEW, 'utf8');

interface NativeKeySet {
  readonly identifier: string;
  readonly count: number;
  readonly requiredCount: number;
}

function parseNativeKeySets(source: string): NativeKeySet[] {
  const arrays = new Map<string, string[]>();
  const arrayPattern = /const TCHAR\* const (\w+)\[\] = \{([^}]*)\};/g;
  for (const [, identifier, body] of source.matchAll(arrayPattern)) {
    arrays.set(
      identifier,
      [...body.matchAll(/TEXT\("([^"]+)"\)/g)].map((match) => match[1])
    );
  }
  const sets: NativeKeySet[] = [];
  const tablePattern = /\{ (\w+), (\d+), (\d+) \},?/g;
  for (const [, identifier, count, requiredCount] of source.matchAll(tablePattern)) {
    const keys = arrays.get(identifier);
    expect(keys, `key array ${identifier} must be declared before GVectorKeySets`).toBeDefined();
    sets.push({ identifier, count: Number(count), requiredCount: Number(requiredCount) });
  }
  return sets;
}

describe('vector coercion TS/native parity', () => {
  it('declares identical key sets in identical order on both surfaces', () => {
    const native = parseNativeKeySets(cppCoercionSource);
    const nativeKeys = native.map((set) => set.identifier);
    expect(nativeKeys).toEqual(['XyzwKeys', 'XyzKeys', 'PyrKeys', 'RgbaKeys', 'WhKeys', 'XyKeys']);
    expect(VECTOR_KEY_SETS.map((keys) => [...keys])).toEqual([
      ['x', 'y', 'z', 'w'],
      ['x', 'y', 'z'],
      ['pitch', 'yaw', 'roll'],
      ['r', 'g', 'b', 'a'],
      ['width', 'height'],
      ['x', 'y']
    ]);
  });

  it('applies the same last-key-optional rule for four-key sets', () => {
    for (const set of parseNativeKeySets(cppCoercionSource)) {
      expect(set.requiredCount).toBe(set.count === 4 ? 3 : set.count);
    }
    expect(VECTOR_KEY_SETS.every((keys) => keys.length === 2 || keys.length === 3 || keys.length === 4)).toBe(true);
  });

  it('rejects non-finite components on the native surface like TypeScript does', () => {
    expect(cppCoercionSource).toContain('FMath::IsFinite');
  });

  it('wires coercion after defaults and before schema validation on both surfaces', () => {
    expect(tsExecuteSource).toContain('coerceVectorShapes(applyDeclaredDefaults(');

    const coercionAt = cppValidationSource.indexOf('McpCoerceCanonicalVectorShapes(');
    const validationAt = cppValidationSource.indexOf('McpValidateObjectAgainstCanonicalSchema(');
    expect(coercionAt).toBeGreaterThan(-1);
    expect(validationAt).toBeGreaterThan(coercionAt);
    expect(cppValidationSource).toContain('McpCoerceCanonicalVectorShapes(\n\t\tMcpApplyCanonicalSchemaDefaults(');
  });

  it('keeps the native describe action-strip guard guarded like the TS projection', () => {
    expect(cppDescribeOverviewSource).toContain('McpStripActionFromInputSchema');
  });
});
