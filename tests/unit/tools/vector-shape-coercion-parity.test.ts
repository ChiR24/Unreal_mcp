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

const TS_EXECUTE = resolve(repoRoot, 'src/server/gateway/gateway-execute-static-check.ts');
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

// Source-contract reads normalize CRLF so the pins hold on Windows checkouts
// and LF CI alike (repo blobs are LF; sibling suites read the same way).
const readSource = (file: string): string => readFileSync(file, 'utf8').replace(/\r\n/gu, '\n');

const cppCoercionSource = readSource(CPP_COERCION);
const cppValidationSource = readSource(CPP_VALIDATION);
const tsExecuteSource = readSource(TS_EXECUTE);
const cppDescribeOverviewSource = readSource(CPP_DESCRIBE_OVERVIEW);

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
    expect(nativeKeys).toEqual(['PyrKeys', 'XyzwKeys', 'XyzKeys', 'RgbaKeys', 'WhKeys', 'XyKeys']);
    expect(VECTOR_KEY_SETS.map((keys) => [...keys])).toEqual([
      ['pitch', 'yaw', 'roll'],
      ['x', 'y', 'z', 'w'],
      ['x', 'y', 'z'],
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

  it('recurses into batch item arrays on the native surface (TS: vector-shape-coercion.test)', () => {
    expect(cppCoercionSource).toContain('TryGetObjectField(TEXT("items"), ItemSchema)');
    expect(cppCoercionSource).toContain('McpCoerceCanonicalVectorShapes(Item, *ItemSchema)');
  });

  it('lets no shorter key set stand in for a longer one the schema declares on either surface', () => {
    // A two-number array for an {x,y,z} schema matched the trailing ['x','y'] set and shipped as {x, y};
    // the handler then defaulted z (create_light location [100, 200] spawned the light at (100, 200, 0)).
    // Pins the call AND the predicate: a longer set that contains this one, with a further declared key.
    const tsCoercionSource = readSource(resolve(repoRoot, 'src/server/gateway/gateway-schema-validate.ts'));
    expect(tsCoercionSource).toContain('if (isShadowedByLongerSet(keys, declared)) continue;');
    expect(tsCoercionSource).toContain('&& keys.every((key) => longer.includes(key))');
    expect(tsCoercionSource).toContain('&& longer.some((key) => !keys.includes(key) && declared.includes(key)));');
    expect(cppCoercionSource).toContain('if (IsShadowedByLongerSet(Set, *Declared)) continue;');
    expect(cppCoercionSource).toContain('bContainsSet = SetHasKey(Longer, Set.Keys[Index]);');
    expect(cppCoercionSource).toContain(
      'if (!SetHasKey(Set, Longer.Keys[Index]) && Declared->HasField(Longer.Keys[Index])) return true;'
    );
  });

  it('coerces only arrays of finite numbers into objects on either surface', () => {
    // The native twin read each element with AsNumber(), so ["a","b","c"] became {x: 0, y: 0, z: 0}
    // (LogJson: "Json Value of type 'String' used as a 'Number'") where TypeScript refuses.
    const tsCoercionSource = readSource(resolve(repoRoot, 'src/server/gateway/gateway-schema-validate.ts'));
    expect(tsCoercionSource).toContain('Array.isArray(value) && value.every(isFiniteNumber)');
    expect(cppCoercionSource).toContain('Value->Type == EJson::Array && AllFiniteNumbers(Value->AsArray())');
    expect(cppCoercionSource).toMatch(
      /bool AllFiniteNumbers\([^)]*\)\n\t\{\n\t\tfor \([^)]*\)\n\t\t\{\n\t\t\tif \(!IsFiniteNumber\(Value\)\) return false;\n\t\t\}\n\t\treturn true;\n\t\}/
    );
  });

  it('keeps the native describe action-strip guard guarded like the TS projection', () => {
    expect(cppDescribeOverviewSource).toContain('McpStripActionFromInputSchema');
  });
});
