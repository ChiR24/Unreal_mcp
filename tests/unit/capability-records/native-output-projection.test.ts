/**
 * tests/unit/capability-records/native-output-projection.test.ts
 *
 * A handler result is projected onto the capability's DECLARED output fields
 * before it is validated and published (projectCanonicalOutput in
 * src/server/gateway/gateway-execute-dispatch.ts; McpProjectCanonicalOutput on
 * the native transport). Validating the raw result against a closed output
 * schema turned correct payloads (compiled, saved, scsVerification, ...) into
 * OUTPUT_SCHEMA_VIOLATION. A genuinely missing required field or a wrong-typed
 * declared field must still fail.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import type { CapabilityRecordSource, Draft202012ObjectSchema } from '../../../src/tools/catalog/capabilities/model.js';
// Per-action contracts are authored on the unfolded records.
import { MANAGE_BLUEPRINT_UNFOLDED_SOURCES as MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { projectCanonicalOutput } from '../../../src/server/gateway/gateway-execute-dispatch.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const read = (relativePath: string): string => readFileSync(resolve(REPO_ROOT, relativePath), 'utf8');

function outputSchemaOf(id: string): Draft202012ObjectSchema {
  const found = MANAGE_BLUEPRINT_RECORDS.find((r: CapabilityRecordSource) => String(r.id) === id);
  if (found === undefined) throw new Error(`no capability record ${id}`);
  return found.schemas.output;
}

const declaredKeys = (schema: Draft202012ObjectSchema): ReadonlySet<string> =>
  new Set(isRecord(schema.properties) ? Object.keys(schema.properties) : []);

// The native completion carries the success verdict beside the payload, so the
// seam stamps it before projecting (the result's own success wins).
const canonical = (raw: Record<string, unknown>, schema: Draft202012ObjectSchema) =>
  projectCanonicalOutput({ success: true, ...raw }, schema) as Record<string, unknown>;

describe('output projection publishes declared fields only', () => {
  it.each([
    ['blueprint.set_scs_property', { success: true, message: 'SCS property set', verifiedValue: true, compiled: true, saved: true, scsVerification: { exists: true } }],
    ['blueprint.set_default', { propertyName: 'InitialHealth', blueprintPath: '/Game/Blueprints/BP_Test', value: 100, assetPath: '/Game/Blueprints/BP_Test', existsAfter: true }],
  ] as const)('%s: the raw result fails the closed schema, the projection passes', (id, raw) => {
    const schema = outputSchemaOf(id);
    expect(validateAgainstCapabilitySchema(raw, schema), `${id} raw payload unexpectedly validated`).toBeDefined();
    const projected = canonical(raw, schema);
    expect(validateAgainstCapabilitySchema(projected, schema)).toBeUndefined();
    const declared = declaredKeys(schema);
    expect(Object.keys(projected).every((key) => declared.has(key))).toBe(true);
    expect('success' in projected).toBe(true);
  });

  it('keeps a missing required field and a wrong-typed field failing', () => {
    const schema = outputSchemaOf('blueprint.add_scs_component');
    expect(schema.required).toContain('componentName');
    expect(validateAgainstCapabilitySchema(canonical({ saved: true }, schema), schema)?.reason).toBe('missing-required');
    expect(validateAgainstCapabilitySchema(canonical({ componentName: 123, saved: true }, schema), schema)?.reason).toBe('type');
  });
});

// The receipt carries the message at its top; a record declaring only {success, details} printed it a second time
// under details.message.
describe('the details fold leaves the message to the receipt', () => {
  const schema: Draft202012ObjectSchema = {
    $schema: 'https://json-schema.org/draft/2020-12/schema', type: 'object',
    properties: { success: { type: 'boolean' }, details: { type: 'object' } }, required: ['success'], additionalProperties: false,
  };

  it('folds the undeclared fields but not the message, from the root or the payload', () => {
    const projected = canonical({ message: 'Collection created', data: { message: 'Collection created', added: ['Wind'] }, created: true }, schema);
    expect(projected.details).toEqual({ created: true, added: ['Wind'] });
  });

  it('on both transports', () => {
    expect(read(`${PLUGIN}/MCP/Gateway/McpNativeGatewayOutputProjection.cpp`))
      .toContain('Entry.Key == TEXT("liveRevisions") || Entry.Key == TEXT("message") || (*Properties)->HasField(Entry.Key)');
  });
});

describe('the native seam applies the same projection', () => {
  it('defines McpProjectCanonicalOutput and publishes the projected output, never the raw Result', () => {
    expect(read(`${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.cpp`)).toContain('McpProjectCanonicalOutput');
    expect(read(`${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.h`)).toContain('McpProjectCanonicalOutput');
    const seam = read(`${PLUGIN}/MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.cpp`);
    expect(seam).toContain('McpProjectCanonicalOutput');
    expect(seam).toMatch(/McpBuildSuccessReceipt\(\s*(?:Conn\.)?CapabilityId,\s*Canonical/);
    expect(seam).not.toMatch(/McpBuildSuccessReceipt\(\s*(?:Conn\.)?CapabilityId,\s*Result\b/);
  });
});
