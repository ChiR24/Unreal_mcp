/**
 * Extraction of the authoritative {tool,action} occurrence list.
 *
 * The single source of truth is now the canonical capability records
 * (the SAME data the generator derives the 23 parent ToolDefinitions from).
 * A legacy-surface record's routing.parentTool + legacyIds[].action is an
 * authoritative occurrence, so the inventory is derived from source records,
 * never hardcoded and never from a hand-authored base.
 *
 * What the inventory audits is the pre-gateway 23-tool surface: the pairs that
 * actually shipped. A capability authored after that migration reuses legacyIds
 * to declare the pair it routes under, which is not a pair anything shipped, so
 * it is excluded here rather than counted into a total describing history.
 */

import type { CapabilityRecord } from '../model.js';
import { ALL_CAPABILITY_RECORDS } from '../records/aggregate.js';
import type { Evidence, OccurrenceRecord, RawRoute } from './types.js';

export interface RawOccurrence {
  readonly tool: string;
  readonly action: string;
  readonly evidence: Evidence;
}

interface ToolSource {
  readonly source: string;
  readonly symbol: string;
}

/**
 * Deterministic tool -> declaration-source map (relative to repo root).
 *
 * The records are built by the per-parent builder modules under
 * src/tools/catalog/capabilities/records/<parent>/; each occurrence's evidence
 * points at the builder that stamps that parent's records, so the inventory
 * stays traceable to source even after the hand-authored base was removed.
 */
const TOOL_SOURCE: Readonly<Record<string, ToolSource>> = {
  manage_tools: {
    source: 'src/tools/catalog/capabilities/records/manage-tools/builder.ts',
    symbol: 'buildManageToolsRecords()',
  },
  manage_asset: {
    source: 'src/tools/catalog/capabilities/records/manage-asset/builder.ts',
    symbol: 'buildManageAssetRecords()',
  },
  manage_blueprint: {
    source: 'src/tools/catalog/capabilities/records/manage-blueprint/builder.ts',
    symbol: 'buildManageBlueprintRecords()',
  },
  control_actor: {
    source: 'src/tools/catalog/capabilities/records/control-actor/builder.ts',
    symbol: 'buildControlActorRecords()',
  },
  control_editor: {
    source: 'src/tools/catalog/capabilities/records/control-editor/builder.ts',
    symbol: 'buildControlEditorRecords()',
  },
  manage_level: {
    source: 'src/tools/catalog/capabilities/records/manage-level/builder.ts',
    symbol: 'buildManageLevelRecords()',
  },
  build_environment: {
    source: 'src/tools/catalog/capabilities/records/build-environment/builder.ts',
    symbol: 'buildBuildEnvironmentRecords()',
  },
  animation_physics: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  system_control: {
    source: 'src/tools/catalog/capabilities/records/system-control/builder.ts',
    symbol: 'buildSystemControlRecords()',
  },
  manage_sequence: {
    source: 'src/tools/catalog/capabilities/records/manage-sequence/builder.ts',
    symbol: 'buildManageSequenceRecords()',
  },
  inspect: {
    source: 'src/tools/catalog/capabilities/records/inspect/builder.ts',
    symbol: 'buildInspectRecords()',
  },
  manage_audio: {
    source: 'src/tools/catalog/capabilities/records/manage-audio/builder.ts',
    symbol: 'buildManageAudioRecords()',
  },
  manage_geometry: {
    source: 'src/tools/catalog/capabilities/records/world/builder.ts',
    symbol: 'buildWorldRecords()',
  },
  manage_pcg: {
    source: 'src/tools/catalog/capabilities/records/world/builder.ts',
    symbol: 'buildWorldRecords()',
  },
  manage_effect: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_gas: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_character: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_combat: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_ai: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_inventory: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_interaction: {
    source: 'src/tools/catalog/capabilities/records/gameplay/builder.ts',
    symbol: 'buildGameplayRecords()',
  },
  manage_networking: {
    source: 'src/tools/catalog/capabilities/records/manage-networking/builder.ts',
    symbol: 'buildManageNetworkingRecords()',
  },
  manage_level_structure: {
    source: 'src/tools/catalog/capabilities/records/world/builder.ts',
    symbol: 'buildWorldRecords()',
  },
};

export class ExtractionError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ExtractionError';
  }
}

/** Extract every legacy-surface {tool,action} occurrence from the records. */
export function extractOccurrences(
  records: readonly CapabilityRecord[] = ALL_CAPABILITY_RECORDS,
): readonly RawOccurrence[] {
  const out: RawOccurrence[] = [];
  for (const rec of records) {
    if (rec.normalization.provenance === 'post-migration') continue;
    const tool = rec.routing.parentTool;
    const src = TOOL_SOURCE[tool];
    if (src === undefined) {
      throw new ExtractionError(`no source mapping for tool ${tool}`);
    }
    const seen = new Set<string>();
    for (const legacy of rec.legacyIds) {
      const action = legacy.action;
      if (seen.has(action)) {
        throw new ExtractionError(
          `tool ${tool} declares duplicate action "${action}" across records`,
        );
      }
      seen.add(action);
      out.push({
        tool,
        action,
        evidence: { source: src.source, symbol: src.symbol, tool },
      });
    }
  }
  return out;
}

/** Build the raw-route ownership record for an occurrence. */
export function buildRawRoute(tool: string, namespace: string): RawRoute {
  return {
    ownerTool: tool,
    surface: 'ts-action-enum',
    status: 'exposed',
    namespace,
  };
}

/** Exported for reuse by the builder; re-exported occurrence shape. */
export type { OccurrenceRecord };
