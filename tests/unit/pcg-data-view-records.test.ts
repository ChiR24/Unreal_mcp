import { describe, expect, it } from 'vitest';
import { PCG_DATA_VIEW_RECORDS } from '../../src/tools/catalog/capabilities/records/world/manage-pcg.data-views.data.js';
import { PCG_READ_RECORDS } from '../../src/tools/catalog/capabilities/records/world/manage-pcg.read.data.js';
import { MANAGE_PCG_FOLDS } from '../../src/tools/catalog/capabilities/records/folds/manage-pcg.folds.js';
import { applyDeclaredDefaults, validateAgainstCapabilitySchema } from '../../src/server/gateway/gateway-schema-validate.js';
import { isRecord } from '../../src/utils/validation/type-guards.js';

const ACTIONS = [
  'list_pcg_data',
  'get_pcg_data_schema',
  'create_pcg_data_view',
  'read_pcg_data',
  'release_pcg_data_view',
] as const;

const INPUT_FIELDS: Readonly<Record<(typeof ACTIONS)[number], readonly string[]>> = {
  list_pcg_data: ['action', 'componentPath', 'offset', 'limit'],
  get_pcg_data_schema: ['action', 'componentPath', 'datasetId', 'domain', 'offset', 'limit'],
  create_pcg_data_view: ['action', 'componentPath', 'datasetId', 'domain', 'fields', 'attributes', 'sourceOffset', 'sourceLimit'],
  read_pcg_data: ['action', 'componentPath', 'viewId', 'offset', 'limit'],
  release_pcg_data_view: ['action', 'componentPath', 'handleId'],
};

const OUTPUT_FIELDS: Readonly<Record<(typeof ACTIONS)[number], readonly string[]>> = {
  list_pcg_data: ['componentPath', 'items', 'total', 'offset', 'limit', 'truncated', 'consistency'],
  get_pcg_data_schema: ['componentPath', 'datasetId', 'schema', 'consistency'],
  create_pcg_data_view: ['componentPath', 'datasetId', 'viewId', 'projection', 'expiresInSeconds', 'consistency'],
  read_pcg_data: ['componentPath', 'viewId', 'projection', 'rows', 'rowCount', 'offset', 'limit', 'hasMore', 'expiresInSeconds', 'consistency'],
  release_pcg_data_view: ['componentPath', 'handleId', 'released'],
};

const OUTPUT_ENVELOPE_FIELDS = new Set(['success', 'message', 'details']);
const recordFor = (action: string) =>
  PCG_DATA_VIEW_RECORDS.find((record) => record.legacyIds[0]?.action === action);

const recordForOrThrow = (action: string) => {
  const record = recordFor(action);
  if (record === undefined) throw new Error(`Missing record for ${action}`);
  return record;
};

const schemaObject = (value: unknown): Record<string, unknown> => {
  if (!isRecord(value)) throw new Error('Expected a schema object.');
  return value;
};

const property = (record: NonNullable<ReturnType<typeof recordFor>>, key: string) =>
  schemaObject(record.schemas.input.properties[key]);

const outputProperty = (record: NonNullable<ReturnType<typeof recordFor>>, key: string) =>
  schemaObject(record.schemas.output.properties[key]);

describe('PCG data-view capability records', () => {
  it('declares the five unique actions in the requested order without aliases', () => {
    const actions = PCG_DATA_VIEW_RECORDS.map((record) => String(record.legacyIds[0]?.action));
    const ids = PCG_DATA_VIEW_RECORDS.map((record) => String(record.id));

    expect(actions).toEqual(ACTIONS);
    expect(new Set(actions).size).toBe(ACTIONS.length);
    expect(new Set(ids).size).toBe(ACTIONS.length);
    expect(PCG_DATA_VIEW_RECORDS.every((record) => record.aliases.length === 0)).toBe(true);
  });

  it('matches the existing inspect_pcg read family and availability contract', () => {
    const inspectFold = MANAGE_PCG_FOLDS.find((fold) => fold.primary === 'inspect_pcg');
    const readMember = PCG_READ_RECORDS.find((record) => record.legacyIds[0]?.action === 'get_pcg_graph');

    expect(inspectFold?.selector).toBe('view');
    expect(inspectFold?.members).toHaveProperty('graph', 'get_pcg_graph');
    expect(readMember?.discovery.family).toBe('pcg-query');

    for (const record of PCG_DATA_VIEW_RECORDS) {
      expect(record.routing.parentTool).toBe('manage_pcg');
      expect(record.discovery.domain).toBe('world');
      expect(record.discovery.family).toBe('pcg-query');
      expect(record.discovery.topics).toHaveLength(4);
      expect(record.behavior.effect).toBe('read');
      expect(record.policy).toEqual(readMember?.policy);
      expect(record.policy).toMatchObject({ requiredScope: 'read', consent: 'none', dataAccess: 'project-read' });
      expect(record.availability).toEqual(readMember?.availability);
      expect(record.availability).toEqual({ requiredPlugins: ['PCG'], editorStates: ['edit'] });
      expect(record.cost).toEqual({ latency: 'interactive', resources: 'medium' });
    }
  });

  it('requires only the exact component and handle inputs for each action', () => {
    const requiredByAction: Readonly<Record<(typeof ACTIONS)[number], readonly string[]>> = {
      list_pcg_data: ['action', 'componentPath'],
      get_pcg_data_schema: ['action', 'componentPath', 'datasetId'],
      create_pcg_data_view: ['action', 'componentPath', 'datasetId'],
      read_pcg_data: ['action', 'componentPath', 'viewId'],
      release_pcg_data_view: ['action', 'componentPath', 'handleId'],
    };

    for (const action of ACTIONS) {
      const record = recordFor(action);
      expect(record).toBeDefined();
      if (record === undefined) throw new Error(`Missing record for ${action}`);

      expect(Object.keys(record.schemas.input.properties)).toEqual(INPUT_FIELDS[action]);
      expect(record.schemas.input.required).toEqual(requiredByAction[action]);
      expect(property(record, 'componentPath')).toMatchObject({ type: 'string' });
      expect(property(record, 'componentPath')).toHaveProperty('description');
      expect(String(property(record, 'componentPath').description)).toContain('Exact current editor');
      expect(String(property(record, 'componentPath').description)).toContain('PCG component');
      expect(String(property(record, 'componentPath').description)).toContain('no actor fallback');

      const forbidden = ['save', 'force', 'expectedGraphRevision', 'operationId', 'taskId', 'timeoutSeconds'];
      expect(forbidden.filter((name) => Object.hasOwn(record.schemas.input.properties, name))).toEqual([]);
    }

    for (const [action, handleName] of [
      ['get_pcg_data_schema', 'datasetId'],
      ['create_pcg_data_view', 'datasetId'],
      ['read_pcg_data', 'viewId'],
      ['release_pcg_data_view', 'handleId'],
    ] as const) {
      const record = recordFor(action);
      if (record === undefined) throw new Error(`Missing record for ${action}`);
      expect(property(record, handleName)).toMatchObject({ type: 'string', maxLength: 64 });
      expect(String(property(record, handleName).description)).toContain('Nonempty');
      expect(property(record, handleName)).not.toHaveProperty('minLength');
    }
  });

  it('bounds list and schema pagination and documents the stated defaults', () => {
    const list = recordFor('list_pcg_data');
    const schema = recordFor('get_pcg_data_schema');
    if (list === undefined || schema === undefined) throw new Error('Missing PCG data list/schema record');

    expect(property(list, 'offset')).toMatchObject({ type: 'integer', minimum: 0, maximum: 256 });
    expect(String(property(list, 'offset')?.description)).toContain('defaults to 0');
    expect(property(list, 'limit')).toMatchObject({ type: 'integer', minimum: 1, maximum: 8 });
    expect(String(property(list, 'limit')?.description)).toContain('defaults to 4');

    expect(property(schema, 'domain')).toMatchObject({ type: 'string', enum: ['Data', 'Elements'] });
    expect(String(property(schema, 'domain')?.description)).toContain('defaults to Elements');
    expect(property(schema, 'offset')).toMatchObject({ type: 'integer', minimum: 0, maximum: 2147483647 });
    expect(String(property(schema, 'offset')?.description)).toContain('defaults to 0');
    expect(property(schema, 'limit')).toMatchObject({ type: 'integer', minimum: 1, maximum: 32 });
    expect(String(property(schema, 'limit')?.description)).toContain('defaults to 16');
  });

  it('bounds selected projections and requires a nonempty explicit supported selection in the handler contract', () => {
    const create = recordFor('create_pcg_data_view');
    if (create === undefined) throw new Error('Missing create data-view record');

    expect(property(create, 'domain')).toMatchObject({ type: 'string', enum: ['Data', 'Elements'] });
    expect(String(property(create, 'domain')?.description)).toContain('defaults to Elements');
    expect(property(create, 'fields')).toMatchObject({ type: 'array', maxItems: 8 });
    expect(property(create, 'fields')).not.toHaveProperty('uniqueItems');
    expect(String(property(create, 'fields').description)).toContain('Unique selected built-in fields');
    expect(String(property(create, 'fields').description)).toContain('native handler rejects duplicates');
    expect(String(property(create, 'fields').description)).toContain('explicit nonempty total selection');
    expect(property(create, 'fields')?.items).toMatchObject({
      type: 'string',
      enum: ['transform', 'density', 'seed', 'boundsMin', 'boundsMax', 'color', 'steepness', 'metadataEntry'],
    });
    expect(String(property(create, 'fields')?.description)).toContain('defaults to empty');
    expect(String(property(create, 'fields')?.description)).toContain('Point transforms use native quaternion xyzw');
    expect(String(property(create, 'fields')?.description)).toContain('Metadata Transform');

    expect(property(create, 'attributes')).toMatchObject({ type: 'array', maxItems: 8 });
    expect(property(create, 'attributes')).not.toHaveProperty('uniqueItems');
    expect(property(create, 'attributes')?.items).toMatchObject({ type: 'string', maxLength: 1023 });
    expect(property(create, 'attributes')?.items).not.toHaveProperty('minLength');
    expect(String(property(create, 'attributes').description)).toContain('Unique nonempty reflected attribute names');
    expect(String(schemaObject(property(create, 'attributes').items).description)).toContain('native column validator enforces nonemptiness');
    expect(String(property(create, 'attributes').description)).toContain('native handler enforces nonemptiness and rejects duplicates');
    expect(String(property(create, 'attributes').description)).toContain('defaults to empty');
    expect(String(property(create, 'attributes').description)).toContain('Unsupported descriptors/columns/refusals do not count');
    expect(String(property(create, 'attributes').description)).toContain('Unsafe Int64 values are decimal strings');
    expect(String(property(create, 'attributes').description)).toContain('soft references remain paths');
    expect(property(create, 'sourceOffset')).toMatchObject({ type: 'integer', minimum: 0, maximum: 2147483647 });
    expect(String(property(create, 'sourceOffset')?.description)).toContain('defaults to 0');
    expect(property(create, 'sourceLimit')).toMatchObject({ type: 'integer', minimum: 1, maximum: 256 });
    expect(String(property(create, 'sourceLimit')?.description)).toContain('defaults to 64');
  });

  it('bounds frozen-view pagination and retains only the requested response fields', () => {
    const read = recordFor('read_pcg_data');
    const release = recordFor('release_pcg_data_view');
    if (read === undefined || release === undefined) throw new Error('Missing PCG data read/release record');

    expect(property(read, 'offset')).toMatchObject({ type: 'integer', minimum: 0, maximum: 256 });
    expect(String(property(read, 'offset')?.description)).toContain('defaults to 0');
    expect(property(read, 'limit')).toMatchObject({ type: 'integer', minimum: 1, maximum: 64 });
    expect(String(property(read, 'limit')?.description)).toContain('defaults to 16');
    expect(property(release, 'handleId')).toMatchObject({ type: 'string', maxLength: 64 });
    expect(property(release, 'handleId')).not.toHaveProperty('minLength');
    expect(String(property(release, 'handleId').description)).toContain('Nonempty');
    expect(String(property(release, 'handleId').description)).toContain('datasetId or viewId');
    expect(outputProperty(recordForOrThrow('get_pcg_data_schema'), 'datasetId')).toMatchObject({ type: 'string', maxLength: 64 });
    expect(outputProperty(recordForOrThrow('create_pcg_data_view'), 'datasetId')).toMatchObject({ type: 'string', maxLength: 64 });
    expect(outputProperty(recordForOrThrow('create_pcg_data_view'), 'viewId')).toMatchObject({ type: 'string', maxLength: 64 });
    expect(outputProperty(read, 'viewId')).toMatchObject({ type: 'string', maxLength: 64 });
    expect(String(outputProperty(read, 'viewId').description)).toContain('deterministically return unavailable');
    expect(outputProperty(release, 'handleId')).toMatchObject({ type: 'string', maxLength: 64 });

    for (const action of ACTIONS) {
      const record = recordFor(action);
      if (record === undefined) throw new Error(`Missing record for ${action}`);
      const actualOutputFields = Object.keys(record.schemas.output.properties).filter((name) => !OUTPUT_ENVELOPE_FIELDS.has(name));
      expect(actualOutputFields).toEqual(OUTPUT_FIELDS[action]);
      expect(record.schemas.output.required).toEqual(['success', ...OUTPUT_FIELDS[action]]);
      expect(record.schemas.output.additionalProperties).toBe(false);
    }

    expect(outputProperty(recordForOrThrow('list_pcg_data'), 'items')).toMatchObject({ type: 'array' });
    expect(outputProperty(recordForOrThrow('list_pcg_data'), 'items').items).toMatchObject({
      type: 'object', 'x-unreal-reflection-boundary': true,
    });
    expect(outputProperty(recordForOrThrow('get_pcg_data_schema'), 'schema')).toMatchObject({ type: 'object', 'x-unreal-reflection-boundary': true });
    expect(outputProperty(recordForOrThrow('create_pcg_data_view'), 'projection')).toMatchObject({ type: 'object', 'x-unreal-reflection-boundary': true });
    expect(outputProperty(recordForOrThrow('read_pcg_data'), 'projection')).toMatchObject({ type: 'object', 'x-unreal-reflection-boundary': true });
    expect(outputProperty(recordForOrThrow('read_pcg_data'), 'rows')).toMatchObject({ type: 'array' });
    const rowDescription = String(outputProperty(recordForOrThrow('read_pcg_data'), 'rows').description);
    expect(rowDescription).toContain('Page-encoded rows are capped at 60,000 characters');
    expect(rowDescription).toContain('complete handler result is capped at 80,000');
    expect(rowDescription).toContain('outer 100,000-character gateway response cap');
    expect(outputProperty(recordForOrThrow('read_pcg_data'), 'rows').items).toMatchObject({
      type: 'object', 'x-unreal-reflection-boundary': true,
    });
  });

  it('keeps historical, coordinate, Int64, and release semantics in the descriptions', () => {
    const create = recordFor('create_pcg_data_view');
    const read = recordFor('read_pcg_data');
    const release = recordFor('release_pcg_data_view');
    if (create === undefined || read === undefined || release === undefined) throw new Error('Missing PCG data-view record');

    const projection = String(outputProperty(create, 'projection').description);
    const rows = String(outputProperty(read, 'rows').description);
    expect(projection).toContain('immutable historical JSON/weak metadata');
    expect(projection).toContain('not a whole dataset');
    expect(projection).toContain('generation/operation ID');
    expect(projection).toContain('sourceOffset, sourceLimit, sourceTotal, rowCount, fields, attributes');
    expect(projection).toContain('attributeTypes[{name,typeId,typeName}]');
    expect(projection).toContain('coordinateSpace is native_stored_no_conversion_bounds_are_point_local');
    expect(projection).toContain('pointRotationEncoding is quaternion_xyzw');
    expect(projection).toContain('attributeTransformRotationEncoding is native_quaternion_to_euler_degrees');
    expect(projection).toContain('provenance is current_component_root_generation_unknown');
    expect(projection).toContain('completeSourceRowRange');
    expect(projection).toContain('Unsafe Int64 values are decimal strings');
    expect(projection).toContain('soft references remain paths');
    expect(projection).not.toContain('sourceCount');
    expect(projection).not.toContain('coordinates:');
    expect(rows).toContain('frozen selected source range only');
    expect(String(outputProperty(read, 'rowCount').description)).toContain('Total number of rows captured');
    expect(String(property(release, 'handleId').description)).toContain('does not clean native PCG resources');

    const list = recordForOrThrow('list_pcg_data');
    const schema = recordForOrThrow('get_pcg_data_schema');
    expect(list.examples[0]?.output.items).toEqual([{
      datasetIndex: 0,
      pin: 'Out',
      dataClass: '/Script/PCG.PCGPointData',
      kind: 'point',
      tags: [],
      datasetId: 'dataset-id-from-list',
      available: true,
    }]);
    expect(list.examples[0]?.output.consistency).toBe('per_call_live_root_inventory_not_cross_page_revision');
    expect(schema.examples[0]?.output.consistency).toBe('per_call_live_schema_not_saved_view');
    expect(create.examples[0]?.output.consistency).toBe('immutable_historical_projection_current_inputs_unknown');
    expect(read.examples[0]?.output.consistency).toBe('immutable_historical_projection_current_inputs_unknown');
    expect(create.examples[0]?.output.expiresInSeconds).toBe(45);
    expect(read.examples[0]?.output.expiresInSeconds).toBe(44);
    expect(read.examples[0]?.output.rowCount).toBe(2);
    expect(read.examples[0]?.output.rows).toHaveLength(1);
    expect(read.examples[0]?.output.hasMore).toBe(true);
    expect(create.examples[0]?.output.projection).toMatchObject({
      sourceOffset: 0,
      sourceLimit: 64,
      sourceTotal: 2,
      rowCount: 2,
      fields: ['transform', 'density'],
      attributes: ['Biome'],
      attributeTypes: [{ name: 'Biome', typeId: 12, typeName: 'Name' }],
      coordinateSpace: 'native_stored_no_conversion_bounds_are_point_local',
      pointRotationEncoding: 'quaternion_xyzw',
      attributeTransformRotationEncoding: 'native_quaternion_to_euler_degrees',
      provenance: 'current_component_root_generation_unknown',
      completeSourceRowRange: true,
    });
    const projectionExample = create.examples[0]?.output.projection;
    if (!isRecord(projectionExample)) throw new Error('Create example projection must be an object.');
    expect(projectionExample).not.toHaveProperty('sourceCount');
    expect(projectionExample).not.toHaveProperty('coordinates');
    expect(read.examples[0]?.output.projection).toMatchObject({ rowCount: 2, sourceTotal: 2 });
    expect(read.examples[0]?.output.rows).toEqual([{
      sourceIndex: 0,
      fields: {
        transform: {
          location: { x: 0, y: 0, z: 0 },
          rotation: { x: 0, y: 0, z: 0, w: 1 },
          scale: { x: 1, y: 1, z: 1 },
        },
        density: 1,
      },
      attributes: { Biome: 'Forest' },
    }]);

    for (const record of PCG_DATA_VIEW_RECORDS) {
      expect(record.discovery.whenToUse.join(' ')).toContain('editor-local CPU read on Unreal 5.7+');
      expect(record.discovery.whenToUse.join(' ')).toContain('unpartitioned, non-runtime-managed');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('before 5.7');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('PCG generation');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('enabling native execution-inspection/capture state');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('GPU transfer');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('saving');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('Partitioned or runtime-managed PCG components');
      expect(record.discovery.whenNotToUse.join(' ')).not.toContain('Do not use for PCG data inspection');
      expect(record.discovery.whenNotToUse.join(' ')).toContain('bounded historical JSON and weak metadata');
    }
  });

  it('keeps portable examples within each record input/output schema shape', () => {
    for (const action of ACTIONS) {
      const record = recordFor(action);
      if (record === undefined) throw new Error(`Missing record for ${action}`);
      const example = record.examples[0];
      if (example === undefined) throw new Error(`Missing example for ${action}`);

      expect(example.input.action).toBe(action);
      expect(example.input.componentPath).toBe('/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent');
      expect(Object.keys(example.input).every((name) => Object.hasOwn(record.schemas.input.properties, name))).toBe(true);
      expect(record.schemas.input.required.every((name) => Object.hasOwn(example.input, name))).toBe(true);
      expect(Object.keys(example.output).every((name) => Object.hasOwn(record.schemas.output.properties, name))).toBe(true);
      expect(Object.keys(example.output)).toEqual(['success', ...OUTPUT_FIELDS[action]]);
      expect(example.output.success).toBe(true);

      const validatedInput = applyDeclaredDefaults(example.input, record.schemas.input);
      const inputViolation = validateAgainstCapabilitySchema(validatedInput, record.schemas.input);
      expect(inputViolation, `${action} example input schema violation: ${JSON.stringify(inputViolation)}`).toBeUndefined();
      const outputViolation = validateAgainstCapabilitySchema(example.output, record.schemas.output);
      expect(outputViolation, `${action} example output schema violation: ${JSON.stringify(outputViolation)}`).toBeUndefined();

      const examplePath = String(example.input.componentPath);
      expect(examplePath.startsWith('/Game/')).toBe(true);
      expect(examplePath).not.toContain('C:/');
      expect(examplePath).not.toContain('\\');
    }

    const createExample = recordForOrThrow('create_pcg_data_view').examples[0]?.input;
    expect(createExample?.fields).toEqual(['transform', 'density']);
    expect(createExample?.attributes).toEqual(['Biome']);
    if (!Array.isArray(createExample?.fields) || !Array.isArray(createExample?.attributes)) {
      throw new Error('Create example must select fields and attributes as arrays.');
    }
    expect(new Set(createExample.fields).size).toBe(2);
    expect(new Set(createExample.attributes).size).toBe(1);
  });
});
