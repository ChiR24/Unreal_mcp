import type { CapabilityRecordSource } from '../../model.js';

/** The shared receipt of the existing add, configure and connect implementations. */
export function withPCGEditContract(record: CapabilityRecordSource): CapabilityRecordSource {
  const action = record.legacyIds[0]?.action ?? '';
  if (!action.startsWith('add_') && action !== 'connect_pcg_pins' && action !== 'set_pcg_node_settings') return record;
  return {
    ...record,
    schemas: {
      input: {
        ...record.schemas.input,
        properties: {
          ...record.schemas.input.properties,
          expectedGraphRevision: { type: 'string', description: 'Optional authored graph revision from get_pcg_graph. A stale revision is refused before mutation.' },
        },
      },
      output: {
        ...record.schemas.output,
        properties: {
          ...record.schemas.output.properties,
          committed: { type: 'boolean', description: 'The requested in-memory edit committed; this does not imply saving.' },
          saved: { type: 'boolean', description: 'The edited graph was saved successfully.' },
          rolledBack: { type: 'boolean', description: 'Explicit failure recovery restored the pre-edit graph snapshot.' },
          residualChanges: { type: 'boolean', description: 'Failure recovery could not verify complete restoration.' },
          revision: { type: 'string', description: 'Authored graph content revision after a successful edit.' },
          editorGraph: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Editor/model mapping status after the edit. An unopened graph is reported as not_created.' },
          undo: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Honest editor undo availability, separate from explicit failure recovery and disk persistence.' },
        },
      },
    },
  };
}
