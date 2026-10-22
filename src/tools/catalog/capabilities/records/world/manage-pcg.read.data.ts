import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';

const page: JsonObject = {
  offset: { type: 'integer', minimum: 0, description: 'First item index, default zero.' },
  limit: { type: 'integer', minimum: 1, maximum: 100, description: 'Maximum items, default 50.' },
};
const graph: JsonObject = {
  graphPath: { type: 'string', description: 'PCG graph asset path.' },
  nodeId: { type: 'string', description: 'Only return the node with this exact identifier.' },
  includeSettings: { type: 'boolean', description: 'Include reflected authored settings and owned subobjects, default false.' },
  edgeOffset: { type: 'integer', minimum: 0, description: 'First edge index, default zero.' },
  edgeLimit: { type: 'integer', minimum: 1, maximum: 1000, description: 'Maximum edges, default 500.' },
};
const output: JsonObject = {
  items: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Paged node, class or property records.' },
  total: { type: 'integer', description: 'Item count before pagination.' },
  offset: { type: 'integer', description: 'First item index.' },
  limit: { type: 'integer', description: 'Maximum returned items.' },
  truncated: { type: 'boolean', description: 'More items remain after this page.' },
  revision: { type: 'string', description: 'In-process authored graph content fingerprint, not a catalog revision.' },
  editorGraph: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Model/editor mapping diagnostics including hidden nodes, comments and edge consistency.' },
  editorConsistent: { type: 'boolean', description: 'The existing editor representation maps consistently to visible model nodes and edges.' },
  graphPath: { type: 'string', description: 'Resolved graph path.' },
  edges: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Paged backend or editor edges.' },
  edgeTotal: { type: 'integer', description: 'Total edges.' },
  edgeOffset: { type: 'integer', description: 'First edge index.' },
  edgesTruncated: { type: 'boolean', description: 'More edges remain.' },
  available: { type: 'boolean', description: 'An editor graph already exists; a read never opens one.' },
  ambiguous: { type: 'boolean', description: 'Multiple owned editor graph candidates prevent a safe read.' },
  valid: { type: 'boolean', description: 'All inspected model edges pass native pin compatibility checks.' },
  validationScope: { type: 'string', description: 'Checks performed; this is not a generation result.' },
  diagnostics: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Paged validation diagnostics.' },
  diagnosticTotal: { type: 'integer', description: 'Total validation diagnostics.' },
  loadedClassesOnly: { type: 'boolean', description: 'Class discovery only enumerates loaded concrete settings classes.' },
  settingsClass: { type: 'string', description: 'Resolved settings class.' },
};
const entries: readonly { action: string; summary: string; topics: readonly string[]; inputProps: JsonObject; required: readonly string[] }[] = [
  { action: 'list_pcg_node_types', summary: 'Discover loaded concrete PCG settings classes.', topics: ['find pcg node types', 'list pcg classes', 'pcg node discovery'],
    inputProps: { ...page, filter: { type: 'string', description: 'Case-insensitive settings class path filter.' } }, required: [] },
  { action: 'get_pcg_settings_schema', summary: 'Read reflected editable PCG settings property types and defaults.', topics: ['pcg settings schema', 'pcg property defaults', 'inspect pcg node configuration'],
    inputProps: { ...page, settingsClass: { type: 'string', description: 'Concrete PCG settings class path or existing alias.' } }, required: ['settingsClass'] },
  ...['get_pcg_graph', 'get_pcg_editor_graph', 'validate_pcg_graph'].map(action => ({
    action, summary: action === 'get_pcg_editor_graph' ? 'Read the existing PCG editor representation without opening or modifying it.' :
      action === 'validate_pcg_graph' ? 'Validate native pin compatibility of backend PCG edges without mutating the graph.' : 'Read PCG nodes, settings, pins, positions, edges and a content revision.',
    topics: ['read pcg graph', 'inspect pcg connections', 'pcg graph validation'],
    inputProps: { ...page, ...graph }, required: ['graphPath'],
  })),
];
export const PCG_READ_RECORDS: readonly CapabilityRecordSource[] = entries.map(entry => buildWorldRecord({
  parentTool: 'manage_pcg', ...entry, plugins: ['PCG'], family: 'pcg-query',
  whenToUse: ['Discover or inspect PCG state before editing.'], whenNotToUse: ['Generation completion or asset mutation is required.'],
  effect: 'read', costLatency: 'interactive', costResources: 'medium', outputProps: output,
  exampleInput: { action: entry.action, ...(entry.required.includes('graphPath') ? { graphPath: '/Game/PCG/Graph' } : {}),
    ...(entry.required.includes('settingsClass') ? { settingsClass: 'PCGTransformPointsSettings' } : {}) },
}));
