import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';

const target: JsonObject = {
  target: { type: 'string', enum: ['graph', 'graphInstance', 'component'], description: 'Parameter owner, default graph. Supply only the matching target path.' },
  graphPath: { type: 'string', description: 'Graph asset path for target=graph.' },
  graphInstancePath: { type: 'string', description: 'Graph instance asset path for target=graphInstance.' },
  componentPath: { type: 'string', description: 'Exact absolute PCG component object path for target=component. Short names are not accepted.' },
};
const write: JsonObject = {
  ...target,
  name: { type: 'string', description: 'Existing parameter name, or the new name for an add operation.' },
  expectedGraphRevision: { type: 'string', description: 'Optional authored graph revision; stale graph state is refused before editing.' },
  save: { type: 'boolean', description: 'Save the graph, graph instance asset or owning component level, default true. Does not imply other instances were saved.' },
};
const output: JsonObject = {
  target: { type: 'string', description: 'Resolved parameter owner category.' },
  graphPath: { type: 'string', description: 'Parent PCG graph path.' },
  graphInstancePath: { type: 'string', description: 'Resolved graph instance asset path.' },
  componentPath: { type: 'string', description: 'Resolved component object path.' },
  parameters: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Parameter names, stable IDs, native types, container, default/effective values and override flags. Int64 values outside the JSON safe-integer range are exact decimal strings, including array elements.' },
  revision: { type: 'string', description: 'Parent graph authored revision; not an instance override version.' },
  total: { type: 'integer', description: 'Total matching parameters before pagination.' },
  offset: { type: 'integer', description: 'First parameter index.' },
  limit: { type: 'integer', description: 'Maximum returned parameters.' },
  truncated: { type: 'boolean', description: 'More parameter rows remain.' },
  committed: { type: 'boolean', description: 'The native in-memory parameter operation completed.' },
  mayHaveCommitted: { type: 'boolean', description: 'A native commit failure may have left a schema or override change; inspect the target.' },
  saved: { type: 'boolean', description: 'The selected owner was saved.' },
  rolledBack: { type: 'boolean', description: 'Cross-instance parameter propagation is not claimed to be atomically rolled back.' },
  rollbackScope: { type: 'string', description: 'Explicit persistence and rollback boundary.' },
  references: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Loaded parameter getter references that prevent removal.' },
  loadedReferencesOnly: { type: 'boolean', description: 'Reference diagnostics inspect loaded graphs only; no project-wide unused-parameter claim.' },
  editorGraph: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Editor consistency after parameter propagation.' },
  undo: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Honest editor undo availability.' },
};
const specs: readonly { action: string; effect: 'read' | 'write' | 'destructive'; family: string; summary: string; inputProps: JsonObject; required: readonly string[] }[] = [
  { action: 'get_pcg_parameters', effect: 'read', family: 'pcg-query', summary: 'Read native PCG parameter schema, stable IDs, defaults, effective values and instance overrides.',
    inputProps: { ...target, name: { type: 'string', description: 'Optional exact parameter-name filter.' },
      offset: { type: 'integer', minimum: 0, description: 'First parameter, default zero.' },
      limit: { type: 'integer', minimum: 1, maximum: 100, description: 'Maximum parameters, default 50.' } }, required: [] },
  { action: 'add_pcg_parameter', effect: 'write', family: 'pcg-parameters', summary: 'Add a native graph parameter with a stable ID and optional initial default.',
    inputProps: { ...write,
      type: { type: 'string', enum: ['Bool', 'Byte', 'Int32', 'Int64', 'Float', 'Double', 'Name', 'String', 'Enum', 'Struct', 'Object', 'Class', 'SoftObject', 'SoftClass'], description: 'Native property bag value type.' },
      typeObject: { type: 'string', description: 'Enum, struct or class path. Struct supports Vector, Rotator and Transform.' },
      container: { type: 'string', enum: ['none', 'array'], description: 'Scalar or one array container, default none.' },
      value: { description: 'Optional initial default value, validated on a copied property bag before adding the parameter. Supply Int64 values outside the JSON safe-integer range as decimal strings.' } }, required: ['name', 'type', 'graphPath'] },
  { action: 'set_pcg_parameter', effect: 'write', family: 'pcg-parameters', summary: 'Set a graph default or an explicit instance override using native typed parameter setters.',
    inputProps: { ...write, value: { description: 'New typed value. Arrays are limited to 10000 elements. Supply Int64 values outside the JSON safe-integer range as decimal strings. Null explicitly clears an object reference.' } }, required: ['name', 'value'] },
  { action: 'rename_pcg_parameter', effect: 'write', family: 'pcg-parameters', summary: 'Rename a graph parameter through the native API while retaining its stable ID and native references.',
    inputProps: { ...write, newName: { type: 'string', description: 'Unused new parameter name.' } }, required: ['name', 'newName'] },
  { action: 'reset_pcg_parameter_override', effect: 'write', family: 'pcg-parameters', summary: 'Clear a single graph-instance or component override and resume inheritance of the parent value.',
    inputProps: write, required: ['name'] },
  { action: 'remove_pcg_parameter', effect: 'destructive', family: 'pcg-parameter-removal', summary: 'Remove a graph parameter only when no loaded getter node references it; never cascade delete nodes.',
    inputProps: write, required: ['name'] },
];
export const PCG_PARAMETER_RECORDS: readonly CapabilityRecordSource[] = specs.map(spec => buildWorldRecord({
  parentTool: 'manage_pcg', ...spec, plugins: ['PCG'],
  topics: ['pcg graph parameters', 'pcg component overrides', 'pcg parameter defaults', 'reset pcg override'],
  whenToUse: ['Inspect or parameterize reusable PCG graphs and independent instances on engines with supported native parameter APIs.'],
  whenNotToUse: ['Atomic node batches or asynchronous generation are needed. Cross-instance propagation is not a single-graph atomic edit.'],
  costLatency: 'interactive', costResources: 'medium', outputProps: output,
  exampleInput: { action: spec.action, target: spec.action === 'reset_pcg_parameter_override' ? 'graphInstance' : 'graph',
    ...(spec.action === 'reset_pcg_parameter_override' ? { graphInstancePath: '/Game/PCG/GraphInstance' } : { graphPath: '/Game/PCG/Graph' }),
    ...(spec.action === 'get_pcg_parameters' ? {} : { name: 'Spacing' }),
    ...(spec.action === 'add_pcg_parameter' ? { type: 'Double', value: 100 } : spec.action === 'set_pcg_parameter' ? { value: 200 } :
      spec.action === 'rename_pcg_parameter' ? { newName: 'PointSpacing' } : {}) },
}));
