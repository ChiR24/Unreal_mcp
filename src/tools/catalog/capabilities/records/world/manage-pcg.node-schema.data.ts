import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';

export const PCG_NODE_SCHEMA_RECORDS: readonly CapabilityRecordSource[] = [buildWorldRecord({
  parentTool: 'manage_pcg', action: 'get_pcg_node_schema', family: 'pcg-query', plugins: ['PCG'],
  summary: 'Discover metadata for an existing PCG node instance or its initialized owned selector without loading or mutation.',
  topics: ['pcg node instance schema', 'inspect initialized pcg selector', 'pcg owned settings properties'],
  whenToUse: ['Discover enum, reference and array descriptors on a loaded graph node before configuring its existing settings.'],
  whenNotToUse: ['Class/CDO defaults are required: use get_pcg_settings_schema.',
    'The graph is unloaded, a selector must be initialized, or property values, mutation or generation are required.'],
  effect: 'read', costLatency: 'interactive', costResources: 'medium',
  inputProps: {
    graphPath: { type: 'string', maxLength: 2048, description: 'Exact already-loaded PCG graph asset path; this read does not load assets or classes.' },
    nodeId: { type: 'string', maxLength: 1023, description: 'Exact backend node name returned by graph discovery/add. Titles, numeric indices and boundary aliases are refused.' },
    scopePath: { type: 'string', maxLength: 512, default: '', description: 'Existing relative settings scope, defaults to settings itself. At most eight dotted property names traversing by-value structs or hard subobjects owned by these settings. Null/shared/CDO/class/soft references and arrays/indices are refused; no initialization or load.' },
    offset: { type: 'integer', minimum: 0, maximum: 2147483647, default: 0, description: 'First sorted editable-property descriptor, defaults to zero.' },
    limit: { type: 'integer', minimum: 1, maximum: 32, default: 16, description: 'Property descriptors per page, defaults to 16; total output and cooperative time budgets also apply.' },
  },
  required: ['graphPath', 'nodeId'],
  outputProps: {
    graphPath: { type: 'string', description: 'Normalized graph asset path.' },
    nodeId: { type: 'string', description: 'Exact backend node identity.' },
    revision: { type: 'string', description: 'Existing authored graph fingerprint, not a schema/value snapshot or world-input version.' },
    settingsClass: { type: 'string', description: 'Actual node settings instance class, not a guessed selector CDO.' },
    scopePath: { type: 'string', description: 'Canonical relative scope path; empty denotes the settings instance.' },
    scopeType: { type: 'string', description: 'Actual already-existing class or struct type path for this scope.' },
    scopeKind: { type: 'string', enum: ['settings', 'owned_object', 'struct'], description: 'Existing instance scope category.' },
    schemaSource: { type: 'string', description: 'existing_node_instance_metadata_only; class defaults and values are not included.' },
    isBlueprintSettings: { type: 'boolean', description: 'The root settings class was compiled from Blueprint; read metadata is not callback sandboxing.' },
    editPolicy: { type: 'string', enum: ['existing_native_staged_preflight_required', 'blueprint_callbacks_not_sandboxed'], description: 'Existing mutation preflight boundary, never blanket setter/atomic-edit support.' },
    metadataOnly: { type: 'boolean', description: 'True: property metadata only, not values or an authorization grant.' },
    valuesIncluded: { type: 'boolean', description: 'False: use separate existing value reads; this action does not serialize instance values.' },
    loadedObjectsOnly: { type: 'boolean', description: 'True: no graph/class/soft-reference load or selector initialization.' },
    consistency: { type: 'string', description: 'per_call_instance_schema_not_value_snapshot.' },
    schema: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Bounded metadata-only scope/items/total/offset/limit/truncated. Sorted descriptors include name/type/category/declaredIn/readOnly/editable/instancedReference and shallow typeDetails for enums, class constraints, structs and arrays/sets/maps (depth <=3). Up to 64 usable enum options with honest totals, separate hidden/MAX omission and exact decimal strings outside safe Int64 JSON range. Reflection edit flags do not promise actual setter support. At most 1024 visited / 256 eligible properties, 1024 characters per metadata string and 60000 encoded schema characters.' },
  },
  exampleInput: { action: 'get_pcg_node_schema', graphPath: '/Game/PCG/Graph', nodeId: 'StaticMeshSpawner_0', scopePath: 'MeshSelectorParameters', limit: 16 },
})];
