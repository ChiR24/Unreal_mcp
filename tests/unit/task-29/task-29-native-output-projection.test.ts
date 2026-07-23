/**
 * tests/unit/task-29/task-29-native-output-projection.test.ts
 *
 * TASK 29 GATE - zero tolerance. The native `/mcp` gateway seam must project a
 * handler result to the capability's DECLARED output fields before it validates
 * and publishes it, exactly as the TypeScript gateway does
 * (src/server/gateway/gateway-execute-dispatch.ts projectCanonicalOutput).
 *
 * The review found the asymmetry: native validated the RAW handler Result
 * against the closed (additionalProperties:false) output schema, so the
 * undeclared transport/verification fields a real handler emits (compiled,
 * saved, scsVerification, propertyName, blueprintPath, assetPath, existsAfter,
 * ...) turned a correct success payload into OUTPUT_SCHEMA_VIOLATION. A second
 * asymmetry: the native completion carries the success verdict separately from
 * the payload (unlike the WebSocket frame the TS gateway projects, which embeds
 * it), so the literal set_default path - which never writes `success` into its
 * Result - additionally failed the required `success` field.
 *
 * The desired behavior, proven here against the shipping native-validator mirror
 * (tests/unit/task-27-suite/schema-subset.ts), publishes ONLY declared fields
 * and passes, while a genuinely missing required field or a wrong-typed declared
 * field still fails.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import type { CapabilityRecordSource } from '../../../src/tools/catalog/capabilities/index.js';
import { MANAGE_ASSET_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-asset/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { validateAgainstSubset } from '../task-27-suite/schema-subset.js';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const SEAM = `${PLUGIN}/MCP/Transport/McpNativeTransportPendingRequests.cpp`;
const VALIDATION_CPP = `${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.cpp`;
const VALIDATION_H = `${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.h`;

type JsonRecord = Record<string, unknown>;

const isRecord = (v: unknown): v is JsonRecord =>
  typeof v === 'object' && v !== null && !Array.isArray(v);
const plain = (v: unknown): JsonRecord => JSON.parse(JSON.stringify(v)) as JsonRecord;
const read = (relativePath: string): string => readFileSync(resolve(REPO_ROOT, relativePath), 'utf8');

/** The exact projection the native seam must implement, mirrored from
 *  gateway-execute-dispatch.ts projectCanonicalOutput and native
 *  McpProjectCanonicalOutput: keep only declared output properties, read each
 *  from the result root, else from its nested `data` payload. */
function projectCanonicalOutput(result: JsonRecord, schema: JsonRecord): JsonRecord {
  const props = isRecord(schema.properties) ? schema.properties : undefined;
  if (props === undefined) return {};
  const payload = isRecord(result.data) ? result.data : undefined;
  const projected: JsonRecord = {};
  for (const name of Object.keys(props)) {
    if (name in result) projected[name] = result[name];
    else if (payload !== undefined && name in payload) projected[name] = payload[name];
  }
  return projected;
}

/** The full native seam behavior: reunite the transport success verdict (the
 *  native completion carries it separately) then project. The result's own
 *  `success` wins when present, matching the seam's stamp-if-absent. */
function nativeCanonicalOutput(result: JsonRecord, schema: JsonRecord): JsonRecord {
  return projectCanonicalOutput({ success: true, ...result }, schema);
}

function recordFor(id: string, records: readonly CapabilityRecordSource[]): CapabilityRecordSource {
  const found = records.find((r) => String(r.id) === id);
  if (found === undefined) throw new Error(`no capability record ${id}`);
  return found;
}

function outputSchemaOf(record: CapabilityRecordSource): JsonRecord {
  return plain(record.schemas.output);
}

const declaredKeys = (schema: JsonRecord): ReadonlySet<string> =>
  new Set(isRecord(schema.properties) ? Object.keys(schema.properties) : []);

// Undeclared fields real native handlers stamp onto a success Result, from the
// SCS/set_default handlers and McpHandlerUtils::AddVerification.
const NATIVE_EXTRAS: JsonRecord = {
  compiled: true,
  saved: true,
  scsVerification: { exists: true },
  propertyName: 'InitialHealth',
  blueprintPath: '/Game/Blueprints/BP_Test',
  assetPath: '/Game/Blueprints/BP_Test',
  assetName: 'BP_Test',
  assetClass: '/Script/Engine.Blueprint',
  existsAfter: true,
};

/** The realistic undeclared native extras for a given output schema. */
function undeclaredExtras(schema: JsonRecord): JsonRecord {
  const declared = declaredKeys(schema);
  return Object.fromEntries(Object.entries(NATIVE_EXTRAS).filter(([key]) => !declared.has(key)));
}

/** First manage_asset record whose output declares a domain field, so this is a
 *  shared-seam proof across parents and not a blueprint special case. */
function pickUnrelatedRecord(): CapabilityRecordSource {
  const found = MANAGE_ASSET_RECORDS.find((r) => {
    const schema = outputSchemaOf(r);
    return schema.additionalProperties === false && declaredKeys(schema).size > 1;
  });
  if (found === undefined) throw new Error('no closed manage_asset output schema with a domain field');
  return found;
}

describe('Task 29 native output projection - the schema-subset mirror matches native', () => {
  const closed: JsonRecord = {
    type: 'object',
    properties: { success: { type: 'boolean' } },
    required: ['success'],
    additionalProperties: false,
  };

  it('rejects an undeclared field, a missing required field, and a wrong type', () => {
    expect(validateAgainstSubset({ success: true, saved: true }, closed)?.reason).toBe('undeclared');
    expect(validateAgainstSubset({}, closed)?.reason).toBe('missing-required');
    expect(validateAgainstSubset({ success: 'yes' }, closed)?.reason).toBe('type');
    expect(validateAgainstSubset({ success: true }, closed)).toBeUndefined();
  });
});

describe('Task 29 native output projection - raw handler payloads violate the closed schema', () => {
  const cases: ReadonlyArray<readonly [string, JsonRecord]> = [
    // set_scs_property emits success + undeclared compiled/saved/scsVerification.
    ['blueprint.set_scs_property',
      { success: true, message: 'SCS property set', verifiedValue: true, compiled: true, saved: true, scsVerification: { exists: true } }],
    // The literal set_default path emits NO success plus undeclared fields.
    ['blueprint.set_default',
      { propertyName: 'InitialHealth', blueprintPath: '/Game/Blueprints/BP_Test', value: 100, assetPath: '/Game/Blueprints/BP_Test', assetName: 'BP_Test', assetClass: '/Script/Engine.Blueprint', existsAfter: true }],
  ];

  it.each(cases)('%s raw success payload is rejected by the native validator', (id, raw) => {
    const schema = outputSchemaOf(recordFor(id, MANAGE_BLUEPRINT_RECORDS));
    const violation = validateAgainstSubset(raw, schema);
    expect(violation, `${id} raw payload unexpectedly validated`).toBeDefined();
    expect(violation && ['undeclared', 'missing-required']).toContain(violation?.reason);
  });

  it('an unrelated capability (manage_asset) is rejected the same way', () => {
    const record = pickUnrelatedRecord();
    const schema = outputSchemaOf(record);
    const raw = { ...plain(record.examples[0]?.output ?? { success: true }), ...undeclaredExtras(schema) };
    expect(Object.keys(undeclaredExtras(schema)).length).toBeGreaterThan(0);
    expect(validateAgainstSubset(raw, schema)?.reason).toBe('undeclared');
  });
});

describe('Task 29 native output projection - canonical projection publishes declared fields and passes', () => {
  const cases: ReadonlyArray<readonly [string, JsonRecord]> = [
    ['blueprint.set_scs_property',
      { success: true, message: 'SCS property set', verifiedValue: true, compiled: true, saved: true, scsVerification: { exists: true } }],
    ['blueprint.set_default',
      { propertyName: 'InitialHealth', blueprintPath: '/Game/Blueprints/BP_Test', value: 100, assetPath: '/Game/Blueprints/BP_Test', existsAfter: true }],
  ];

  it.each(cases)('%s canonical output validates and keeps only declared keys', (id, raw) => {
    const schema = outputSchemaOf(recordFor(id, MANAGE_BLUEPRINT_RECORDS));
    const canonical = nativeCanonicalOutput(raw, schema);
    expect(validateAgainstSubset(canonical, schema), `${id} canonical output failed its own schema`).toBeUndefined();
    const declared = declaredKeys(schema);
    expect(Object.keys(canonical).every((key) => declared.has(key))).toBe(true);
    expect('success' in canonical).toBe(true);
  });

  it('the unrelated manage_asset capability projects to declared keys and passes', () => {
    const record = pickUnrelatedRecord();
    const schema = outputSchemaOf(record);
    const raw = { ...plain(record.examples[0]?.output ?? { success: true }), ...undeclaredExtras(schema) };
    const canonical = nativeCanonicalOutput(raw, schema);
    expect(validateAgainstSubset(canonical, schema)).toBeUndefined();
    const declared = declaredKeys(schema);
    expect(Object.keys(canonical).every((key) => declared.has(key))).toBe(true);
  });
});

describe('Task 29 native output projection - required-field and type failures are preserved', () => {
  it('a missing required domain field still fails after projection', () => {
    const record = recordFor('blueprint.add_scs_component', MANAGE_BLUEPRINT_RECORDS);
    const schema = outputSchemaOf(record);
    expect(schema.required).toContain('componentName');
    const raw = { success: true, saved: true, scsVerification: { exists: true } };
    const canonical = nativeCanonicalOutput(raw, schema);
    expect(validateAgainstSubset(canonical, schema)?.reason).toBe('missing-required');
  });

  it('a wrong-typed declared field still fails after projection', () => {
    const record = recordFor('blueprint.add_scs_component', MANAGE_BLUEPRINT_RECORDS);
    const schema = outputSchemaOf(record);
    const raw = { success: true, componentName: 123, compiled: true, saved: true };
    const canonical = nativeCanonicalOutput(raw, schema);
    expect(validateAgainstSubset(canonical, schema)?.reason).toBe('type');
  });
});

describe('Task 29 native output projection - the native seam applies the projection', () => {
  it('the schema-validation module defines and declares McpProjectCanonicalOutput', () => {
    expect(read(VALIDATION_CPP)).toContain('McpProjectCanonicalOutput');
    expect(read(VALIDATION_H)).toContain('McpProjectCanonicalOutput');
  });

  it('the gateway execute receipt projects before validating and publishes the projected output', () => {
    const seam = read(SEAM);
    expect(seam).toContain('McpProjectCanonicalOutput');
    // The success receipt must publish the projected canonical output, never the raw Result.
    expect(seam).toMatch(/McpBuildSuccessReceipt\(\s*Conn\.CapabilityId,\s*Canonical/);
    expect(seam).not.toMatch(/McpBuildSuccessReceipt\(\s*Conn\.CapabilityId,\s*Result\b/);
  });
});
