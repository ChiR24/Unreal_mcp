import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';

const component = {
  componentPath: {
    type: 'string',
    description: 'Exact current editor unpartitioned, non-runtime-managed PCG component object path; no actor fallback or short-name resolution.'
  },
};
const datasetHandle = {
  type: 'string',
  maxLength: 64,
  description: 'Nonempty opaque dataset handle (1–64 characters) returned by list_pcg_data, bound to its owning caller and exact component; the native handler enforces nonemptiness.',
};
const viewHandle = {
  type: 'string',
  maxLength: 64,
  description: 'Nonempty opaque immutable data-view handle (1–64 characters) returned by create_pcg_data_view, bound to its owning caller and exact component; the native handler enforces nonemptiness and expired or invalidated handles deterministically return unavailable.',
};
const domain = {
  domain: { type: 'string', enum: ['Data', 'Elements'], description: 'PCG data domain to inspect; defaults to Elements.' },
};
const retention = {
  componentPath: { type: 'string', description: 'Exact current editor PCG component object path that owns this caller-local handle; no actor fallback or short-name resolution.' },
  handleId: {
    type: 'string',
    maxLength: 64,
    description: 'Nonempty caller-owned process-local datasetId or viewId to release (1–64 characters); the native handler enforces nonemptiness. This does not clean native PCG resources or grant destructive scope.',
  },
};
const readOnlyWhenNotToUse = [
  'Do not use on Unreal versions before 5.7 or when the exact component is not present in the current editor world.',
  'Do not use for PCG generation, enabling native execution-inspection/capture state, GPU transfer, saving, or claims of world freshness.',
  'Partitioned or runtime-managed PCG components are out of scope; retention is limited to bounded historical JSON and weak metadata, never native dataset capture.',
] as const;

const records: readonly {
  action: string;
  summary: string;
  topics: readonly string[];
  inputProps: JsonObject;
  required: readonly string[];
  outputProps: JsonObject;
  exampleInput: JsonObject;
  exampleOutput: JsonObject;
  whenToUse: readonly string[];
}[] = [
  {
    action: 'list_pcg_data',
    summary: 'List bounded root PCG data entries for one exact current editor component.',
    topics: ['pcg data inventory', 'pcg output datasets', 'pcg component data'],
    inputProps: {
      ...component,
      offset: { type: 'integer', minimum: 0, maximum: 256, description: 'First root entry index; defaults to 0.' },
      limit: { type: 'integer', minimum: 1, maximum: 8, description: 'Maximum root entries; defaults to 4.' },
    },
    required: ['componentPath'],
    outputProps: {
      componentPath: { type: 'string', description: 'Exact current editor PCG component object path read.' },
      items: {
        type: 'array',
        items: {
          type: 'object',
          'x-unreal-reflection-boundary': true,
          description: 'One root datasetIndex/pin/dataClass/kind/tags reflection summary, owner-bound datasetId and availability state.'
        },
        description: 'Bounded root data inventory; each item exposes datasetIndex, pin, dataClass, kind, tags, owner-bound datasetId and available.'
      },
      total: { type: 'integer', description: 'Root inventory entry count before pagination.' },
      offset: { type: 'integer', description: 'First root entry index returned.' },
      limit: { type: 'integer', description: 'Maximum root entries requested.' },
      truncated: { type: 'boolean', description: 'Whether more root entries remain after this page.' },
      consistency: { type: 'string', description: 'Consistency marker per_call_live_root_inventory_not_cross_page_revision; this is not a cross-page content revision.' },
    },
    exampleInput: {
      action: 'list_pcg_data',
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      offset: 0,
      limit: 4,
    },
    exampleOutput: {
      success: true,
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      items: [{ datasetIndex: 0, pin: 'Out', dataClass: '/Script/PCG.PCGPointData', kind: 'point', tags: [], datasetId: 'dataset-id-from-list', available: true }],
      total: 1,
      offset: 0,
      limit: 4,
      truncated: false,
      consistency: 'per_call_live_root_inventory_not_cross_page_revision',
    },
    whenToUse: ['Find the bounded root dataset handles owned by one exact current editor PCG component.'],
  },
  {
    action: 'get_pcg_data_schema',
    summary: 'Read a bounded live reflected schema for one owner-bound PCG dataset.',
    topics: ['pcg data schema', 'pcg dataset columns', 'pcg attribute types'],
    inputProps: {
      ...component,
      datasetId: datasetHandle,
      ...domain,
      offset: { type: 'integer', minimum: 0, maximum: 2147483647, description: 'First schema entry index; defaults to 0.' },
      limit: { type: 'integer', minimum: 1, maximum: 32, description: 'Maximum schema entries; defaults to 16.' },
    },
    required: ['componentPath', 'datasetId'],
    outputProps: {
      componentPath: { type: 'string', description: 'Exact current editor PCG component object path read.' },
      datasetId: { type: 'string', maxLength: 64, description: 'Nonempty owner-bound dataset handle (up to 64 characters) whose live schema was read.' },
      schema: {
        type: 'object',
        'x-unreal-reflection-boundary': true,
        description: 'Bounded reflected domain and attribute types/defaults for the selected dataset domain.',
      },
      consistency: { type: 'string', description: 'Consistency marker per_call_live_schema_not_saved_view; this is a live schema read, not a retained projection.' },
    },
    exampleInput: {
      action: 'get_pcg_data_schema',
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      datasetId: 'dataset-id-from-list',
      domain: 'Elements',
      offset: 0,
      limit: 16,
    },
    exampleOutput: {
      success: true,
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      datasetId: 'dataset-id-from-list',
      schema: { domainName: 'Elements', domain: { name: 'Elements', flag: 2, customFlag: -1, available: true, queried: true }, attributes: [{ name: 'Biome', typeId: 12, typeName: 'Name', defaultAvailable: true, defaultValue: 'Forest' }], attributeTotal: 1, offset: 0, limit: 16, truncated: false },
      consistency: 'per_call_live_schema_not_saved_view',
    },
    whenToUse: ['Inspect bounded reflected attribute types and defaults before selecting a CPU data projection.'],
  },
  {
    action: 'create_pcg_data_view',
    summary: 'Create a bounded immutable CPU projection from selected PCG fields or attributes.',
    topics: ['pcg data projection', 'select pcg data fields', 'pcg materialized view'],
    inputProps: {
      ...component,
      datasetId: datasetHandle,
      ...domain,
      fields: {
        type: 'array',
        maxItems: 8,
        items: { type: 'string', enum: ['transform', 'density', 'seed', 'boundsMin', 'boundsMax', 'color', 'steepness', 'metadataEntry'] },
        description: 'Unique selected built-in fields; the native handler rejects duplicates; defaults to empty, but an explicit nonempty total selection across fields and attributes is required. Point transforms use native quaternion xyzw; Metadata Transform uses the declared native Euler-degrees conversion.'
      },
      attributes: {
        type: 'array',
        maxItems: 8,
        items: { type: 'string', maxLength: 1023, description: 'Nonempty reflected attribute name; the native column validator enforces nonemptiness.' },
        description: 'Unique nonempty reflected attribute names; defaults to empty. The native handler enforces nonemptiness and rejects duplicates. Unsupported descriptors/columns/refusals do not count as supported selections. Unsafe Int64 values are decimal strings and soft references remain paths.',
      },
      sourceOffset: { type: 'integer', minimum: 0, maximum: 2147483647, description: 'First source row; defaults to 0.' },
      sourceLimit: { type: 'integer', minimum: 1, maximum: 256, description: 'Maximum source rows; defaults to 64.' },
    },
    required: ['componentPath', 'datasetId'],
    outputProps: {
      componentPath: { type: 'string', description: 'Exact current editor PCG component object path that owns the view.' },
      datasetId: { type: 'string', maxLength: 64, description: 'Nonempty owner-bound source dataset handle (up to 64 characters).' },
      viewId: { type: 'string', maxLength: 64, description: 'Nonempty caller-owned immutable process-local projection handle (up to 64 characters).' },
      projection: {
        type: 'object',
        'x-unreal-reflection-boundary': true,
        description: 'Frozen projection fields are domain, sourceOffset, sourceLimit, sourceTotal, rowCount, fields, attributes, attributeTypes[{name,typeId,typeName}], coordinateSpace, pointRotationEncoding, attributeTransformRotationEncoding, provenance and completeSourceRowRange. coordinateSpace is native_stored_no_conversion_bounds_are_point_local; pointRotationEncoding is quaternion_xyzw; attributeTransformRotationEncoding is native_quaternion_to_euler_degrees; provenance is current_component_root_generation_unknown. This is bounded immutable historical JSON/weak metadata, not a whole dataset, native dataset capture, world-freshness claim or generation/operation ID; current component inputs are unknown. Unsafe Int64 values are decimal strings and soft references remain paths.'
      },
      expiresInSeconds: { type: 'number', description: 'Remaining time in the 60-second process-local retention TTL.' },
      consistency: { type: 'string', description: 'Consistency marker immutable_historical_projection_current_inputs_unknown; live component inputs may have changed since capture.' },
    },
    exampleInput: {
      action: 'create_pcg_data_view',
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      datasetId: 'dataset-id-from-list',
      domain: 'Elements',
      fields: ['transform', 'density'],
      attributes: ['Biome'],
      sourceOffset: 0,
      sourceLimit: 64,
    },
    exampleOutput: {
      success: true,
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      datasetId: 'dataset-id-from-list',
      viewId: 'view-id-from-create',
      projection: {
        domain: 'Elements',
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
      },
      expiresInSeconds: 45,
      consistency: 'immutable_historical_projection_current_inputs_unknown',
    },
    whenToUse: ['Materialize only selected CPU fields/attributes and a bounded source range for later historical row reads.'],
  },
  {
    action: 'read_pcg_data',
    summary: 'Read bounded typed rows from one retained immutable PCG data view.',
    topics: ['pcg view rows', 'read pcg point data', 'pcg projected attributes'],
    inputProps: {
      ...component,
      viewId: viewHandle,
      offset: { type: 'integer', minimum: 0, maximum: 256, description: 'First frozen-view row; defaults to 0.' },
      limit: { type: 'integer', minimum: 1, maximum: 64, description: 'Maximum rows; defaults to 16.' },
    },
    required: ['componentPath', 'viewId'],
    outputProps: {
      componentPath: { type: 'string', description: 'Exact current editor PCG component object path that owns the view.' },
      viewId: { type: 'string', maxLength: 64, description: 'Nonempty caller-owned immutable process-local projection handle read (up to 64 characters); expired or invalidated handles deterministically return unavailable.' },
      projection: {
        type: 'object',
        'x-unreal-reflection-boundary': true,
        description: 'Frozen projection fields are domain, sourceOffset, sourceLimit, sourceTotal, rowCount, fields, attributes, attributeTypes[{name,typeId,typeName}], coordinateSpace, pointRotationEncoding, attributeTransformRotationEncoding, provenance and completeSourceRowRange. coordinateSpace is native_stored_no_conversion_bounds_are_point_local; pointRotationEncoding is quaternion_xyzw; attributeTransformRotationEncoding is native_quaternion_to_euler_degrees; provenance is current_component_root_generation_unknown. Current component inputs are unknown.'
      },
      rows: {
        type: 'array',
        items: {
          type: 'object',
          'x-unreal-reflection-boundary': true,
          description: 'Typed projected row with sourceIndex and optional selected fields and attributes.',
        },
        description: 'Rows from the frozen selected source range only, shaped as {sourceIndex, fields?, attributes?}. Page-encoded rows are capped at 60,000 characters; the complete handler result is capped at 80,000 before the outer 100,000-character gateway response cap. Unsafe Int64 values are decimal strings; point transforms use native quaternion xyzw; Metadata Transform uses native_quaternion_to_euler_degrees; soft references remain paths.'
      },
      rowCount: { type: 'integer', description: 'Total number of rows captured in the immutable projection, not the rows returned in this page.' },
      offset: { type: 'integer', description: 'First frozen-view row returned.' },
      limit: { type: 'integer', description: 'Maximum rows requested.' },
      hasMore: { type: 'boolean', description: 'Whether rows remain in the frozen selected source range.' },
      expiresInSeconds: { type: 'number', description: 'Remaining time in the 60-second process-local retention TTL.' },
      consistency: { type: 'string', description: 'Consistency marker immutable_historical_projection_current_inputs_unknown; not current world freshness.' },
    },
    exampleInput: {
      action: 'read_pcg_data',
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      viewId: 'view-id-from-create',
      offset: 0,
      limit: 1,
    },
    exampleOutput: {
      success: true,
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      viewId: 'view-id-from-create',
      projection: {
        domain: 'Elements',
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
      },
      rows: [{ sourceIndex: 0, fields: { transform: { location: { x: 0, y: 0, z: 0 }, rotation: { x: 0, y: 0, z: 0, w: 1 }, scale: { x: 1, y: 1, z: 1 } }, density: 1 }, attributes: { Biome: 'Forest' } }],
      rowCount: 2,
      offset: 0,
      limit: 1,
      hasMore: true,
      expiresInSeconds: 44,
      consistency: 'immutable_historical_projection_current_inputs_unknown',
    },
    whenToUse: ['Read only a page of rows from an existing immutable projection while its caller-local handle is valid.'],
  },
  {
    action: 'release_pcg_data_view',
    summary: 'Release only one caller-owned process-local PCG dataset or view retention handle.',
    topics: ['pcg release view', 'release pcg dataset handle', 'pcg retained view memory'],
    inputProps: retention,
    required: ['componentPath', 'handleId'],
    outputProps: {
      componentPath: { type: 'string', description: 'Exact current editor PCG component object path that owns the released handle.' },
      handleId: { type: 'string', maxLength: 64, description: 'Nonempty caller-owned process-local datasetId or viewId addressed (up to 64 characters).' },
      released: { type: 'boolean', description: 'Whether the caller-owned retention handle was released; a root dataset release also releases its row views.' },
    },
    exampleInput: {
      action: 'release_pcg_data_view',
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      handleId: 'view-id-from-create',
    },
    exampleOutput: {
      success: true,
      componentPath: '/Game/Maps/Example.Example:PersistentLevel.PCGActor.PCGComponent',
      handleId: 'view-id-from-create',
      released: true,
    },
    whenToUse: ['Release one caller-owned process-local retention handle; dataset release cascades to its row views.'],
  },
];

export const PCG_DATA_VIEW_RECORDS: readonly CapabilityRecordSource[] = records.map((record) =>
  buildWorldRecord({
    parentTool: 'manage_pcg',
    ...record,
    family: 'pcg-query',
    plugins: ['PCG'],
    editorStates: ['edit'],
    whenToUse: ['Use only as an editor-local CPU read on Unreal 5.7+ for an unpartitioned, non-runtime-managed PCG component; this does not enable native execution-inspection/capture state.', ...record.whenToUse],
    whenNotToUse: readOnlyWhenNotToUse,
    effect: 'read',
    costLatency: 'interactive',
    costResources: 'medium',
    outputRequired: Object.keys(record.outputProps),
  }),
);
