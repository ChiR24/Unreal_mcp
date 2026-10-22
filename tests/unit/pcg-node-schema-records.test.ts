import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { PCG_NODE_SCHEMA_RECORDS } from '../../src/tools/catalog/capabilities/records/world/manage-pcg.node-schema.data.js';
import { MANAGE_PCG_SOURCES } from '../../src/tools/catalog/capabilities/records/world/manage-pcg.index.js';
import { MANAGE_PCG_FOLDS } from '../../src/tools/catalog/capabilities/records/folds/manage-pcg.folds.js';
import { applyDeclaredDefaults, validateAgainstCapabilitySchema } from '../../src/server/gateway/gateway-schema-validate.js';

const record = PCG_NODE_SCHEMA_RECORDS[0];
if (!record) throw new Error('Missing node-instance schema record');
const valid = { action: 'get_pcg_node_schema', graphPath: '/Game/PCG/Graph', nodeId: 'StaticMeshSpawner_0' };

describe('PCG existing node-instance schema contract', () => {
  it('adds one real instance read to the existing family without a new canonical tool', () => {
    expect(PCG_NODE_SCHEMA_RECORDS).toHaveLength(1);
    expect(MANAGE_PCG_SOURCES).toHaveLength(10);
    expect(record.routing.dispatchAction).toBe('get_pcg_node_schema');
    expect(record.policy).toEqual({ requiredScope: 'read', consent: 'none', dataAccess: 'project-read' });
    expect(record.behavior.effect).toBe('read');
    expect(record.availability).toEqual({ requiredPlugins: ['PCG'], editorStates: ['edit'] });
    expect(MANAGE_PCG_FOLDS.find(fold => fold.primary === 'inspect_pcg')?.members)
      .toHaveProperty('node_schema', 'get_pcg_node_schema');
  });

  it('requires exact graph and backend node identity and declares only implemented parameters', () => {
    expect(record.schemas.input.required).toEqual(['action', 'graphPath', 'nodeId']);
    expect(Object.keys(record.schemas.input.properties)).toEqual(['action', 'graphPath', 'nodeId', 'scopePath', 'offset', 'limit']);
    expect(Object.keys(record.schemas.input.properties)).not.toContain('save');
    expect(Object.keys(record.schemas.input.properties)).not.toContain('settingsClass');
    expect(Object.keys(record.schemas.input.properties)).not.toContain('includeValues');
    expect(validateAgainstCapabilitySchema(valid, record.schemas.input)).toBeUndefined();
    for (const missing of ['graphPath', 'nodeId']) {
      const input: Record<string, unknown> = { ...valid };
      delete input[missing];
      expect(validateAgainstCapabilitySchema(input, record.schemas.input)).toBeDefined();
    }
  });

  it('applies declared defaults through the actual gateway validator', () => {
    expect(applyDeclaredDefaults(valid, record.schemas.input)).toMatchObject({ scopePath: '', offset: 0, limit: 16 });
    const example = record.examples[0];
    expect(example).toBeDefined();
    if (!example) throw new Error('Missing instance schema example');
    expect(validateAgainstCapabilitySchema(applyDeclaredDefaults(example.input, record.schemas.input), record.schemas.input)).toBeUndefined();
  });

  it('refuses malformed pages and oversized scope strings without widening the shared validator', () => {
    for (const input of [
      { ...valid, offset: -1 }, { ...valid, offset: 1.25 }, { ...valid, offset: '1' },
      { ...valid, limit: 0 }, { ...valid, limit: 33 }, { ...valid, limit: 1.5 },
      { ...valid, scopePath: 'x'.repeat(513) }, { ...valid, save: true },
    ]) expect(validateAgainstCapabilitySchema(input, record.schemas.input)).toBeDefined();
    expect(validateAgainstCapabilitySchema({ ...valid, scopePath: 'MeshSelectorParameters', offset: 0, limit: 32 }, record.schemas.input)).toBeUndefined();
  });

  // These source assertions pin wiring only; actual schema and ownership behavior is covered by native/workflow tests.
  it('keeps the native read on the existing route and prevents implicit module or object loading', () => {
    const root = new URL('../../plugins/EngineWire/Source/EngineWire/Private/', import.meta.url);
    const parent = readFileSync(new URL('Domains/PCG/McpPCGHandlers.cpp', root), 'utf8');
    const query = readFileSync(new URL('Domains/PCG/Discovery/McpPCGQuery.cpp', root), 'utf8');
    const native = readFileSync(new URL('Domains/PCG/Discovery/Schema/McpPCGNodeSchemaAction.cpp', root), 'utf8');
    const scope = readFileSync(new URL('Domains/PCG/Discovery/Schema/McpPCGSchemaScope.cpp', root), 'utf8');
    expect(query).toContain('return HandlePCGNodeSchema(Bridge, RequestId, Payload, Socket)');
    const refusal = parent.indexOf('SubAction == TEXT("get_pcg_node_schema")');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(parent.indexOf('LoadModulePtr<IModuleInterface>'));
    expect(native).toContain('FindObject<UPCGGraph>');
    expect(native).not.toMatch(/\b(?:LoadObject|StaticLoadObject|McpLoadAsset|LoadPCGAsset|GetDefaultObject|FindPCGNode)\s*(?:<[^>]+>)?\s*\(/);
    expect(native).not.toMatch(/EnableInspection|GenerateLocal|NewObject|FScopedTransaction/);
    expect(scope).toContain('HasOwnedPCGNodeSettings(Graph, Node)');
    expect(scope).toContain('!Object->IsIn(Settings)');
    expect(scope).toContain('ResolveNestedPropertyPath');
  });

  it('preserves the metadata/value and mutation-support distinction in the public receipt', () => {
    expect(Object.keys(record.schemas.output.properties)).toEqual([
      'success', 'message', 'details', 'graphPath', 'nodeId', 'revision', 'settingsClass', 'scopePath', 'scopeType',
      'scopeKind', 'schemaSource', 'isBlueprintSettings', 'editPolicy', 'metadataOnly', 'valuesIncluded',
      'loadedObjectsOnly', 'consistency', 'schema',
    ]);
    expect(record.schemas.output.properties).not.toHaveProperty('defaultValue');
    expect(record.schemas.output.properties).not.toHaveProperty('atomicEditSupported');
  });
});
