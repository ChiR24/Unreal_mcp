/**
 * Extraction of the authoritative {tool,action} occurrence list.
 *
 * The single source of truth is `allToolDefinitions` (the same aggregate the
 * server uses). Every tool's `inputSchema.properties.action.enum` is the
 * canonical surface that declares its action occurrences. We read it directly
 * so the inventory is derived from source, never hardcoded.
 */

import { allToolDefinitions } from '../../../definitions/shared/all-tool-definitions.js';
import type { ToolDefinition } from '../../../definitions/shared/tool-definition.js';
import type { Evidence, OccurrenceRecord, RawRoute } from './types.js';

interface ToolSource {
  readonly source: string;
  readonly symbol: string;
}

/** Deterministic tool -> declaration-source map (relative to repo root). */
const TOOL_SOURCE: Readonly<Record<string, ToolSource>> = {
  manage_tools: {
    source: 'src/tools/definitions/core/manage-tools-tool.ts',
    symbol: 'manageToolsToolDefinition.inputSchema.properties.action.enum',
  },
  manage_asset: {
    source: 'src/tools/definitions/core/manage-asset-tool.ts',
    symbol: 'manageAssetToolDefinition.inputSchema.properties.action.enum',
  },
  manage_blueprint: {
    source: 'src/tools/definitions/core/blueprint/manage-blueprint-tool.ts',
    symbol: 'manageBlueprintToolDefinition.inputSchema.properties.action.enum',
  },
  control_actor: {
    source: 'src/tools/definitions/core/control-actor-tool.ts',
    symbol: 'controlActorToolDefinition.inputSchema.properties.action.enum',
  },
  control_editor: {
    source: 'src/tools/definitions/core/control-editor-tool.ts',
    symbol: 'controlEditorToolDefinition.inputSchema.properties.action.enum',
  },
  manage_level: {
    source: 'src/tools/world/manage-level-tool.ts',
    symbol: 'manageLevelToolDefinition.inputSchema.properties.action.enum',
  },
  build_environment: {
    source: 'src/tools/world/build-environment-tool.ts',
    symbol: 'buildEnvironmentToolDefinition.inputSchema.properties.action.enum',
  },
  animation_physics: {
    source: 'src/tools/definitions/gameplay/animation-physics-tool.ts',
    symbol: 'animationPhysicsToolDefinition.inputSchema.properties.action.enum',
  },
  system_control: {
    source: 'src/tools/definitions/core/system-control-tool.ts',
    symbol: 'systemControlToolDefinition.inputSchema.properties.action.enum',
  },
  manage_sequence: {
    source: 'src/tools/definitions/utility/manage-sequence-tool.ts',
    symbol: 'manageSequenceToolDefinition.inputSchema.properties.action.enum',
  },
  inspect: {
    source: 'src/tools/definitions/core/inspect-tool.ts',
    symbol: 'inspectToolDefinition.inputSchema.properties.action.enum',
  },
  manage_audio: {
    source: 'src/tools/definitions/utility/manage-audio-tool.ts',
    symbol: 'manageAudioToolDefinition.inputSchema.properties.action.enum',
  },
  manage_geometry: {
    source: 'src/tools/world/manage-geometry-tool.ts',
    symbol: 'manageGeometryToolDefinition.inputSchema.properties.action.enum',
  },
  manage_pcg: {
    source: 'src/tools/world/manage-pcg-tool.ts',
    symbol: 'managePcgToolDefinition.inputSchema.properties.action.enum',
  },
  manage_effect: {
    source: 'src/tools/definitions/utility/manage-effect-tool.ts',
    symbol: 'manageEffectToolDefinition.inputSchema.properties.action.enum',
  },
  manage_gas: {
    source: 'src/tools/definitions/gameplay/manage-gas-tool.ts',
    symbol: 'manageGasToolDefinition.inputSchema.properties.action.enum',
  },
  manage_character: {
    source: 'src/tools/definitions/gameplay/manage-character-tool.ts',
    symbol: 'manageCharacterToolDefinition.inputSchema.properties.action.enum',
  },
  manage_combat: {
    source: 'src/tools/definitions/gameplay/manage-combat-tool.ts',
    symbol: 'manageCombatToolDefinition.inputSchema.properties.action.enum',
  },
  manage_ai: {
    source: 'src/tools/definitions/gameplay/ai/manage-ai-tool.ts',
    symbol: 'manageAiToolDefinition.inputSchema.properties.action.enum',
  },
  manage_inventory: {
    source: 'src/tools/definitions/gameplay/manage-inventory-tool.ts',
    symbol: 'manageInventoryToolDefinition.inputSchema.properties.action.enum',
  },
  manage_interaction: {
    source: 'src/tools/definitions/gameplay/manage-interaction-tool.ts',
    symbol: 'manageInteractionToolDefinition.inputSchema.properties.action.enum',
  },
  manage_networking: {
    source: 'src/tools/definitions/utility/networking/manage-networking-tool.ts',
    symbol: 'manageNetworkingToolDefinition.inputSchema.properties.action.enum',
  },
  manage_level_structure: {
    source: 'src/tools/world/manage-level-structure-tool.ts',
    symbol: 'manageLevelStructureToolDefinition.inputSchema.properties.action.enum',
  },
};

export class ExtractionError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ExtractionError';
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function readActionEnum(def: ToolDefinition): readonly string[] {
  const properties = def.inputSchema.properties;
  if (!isRecord(properties)) {
    throw new ExtractionError(`tool ${def.name} has no inputSchema.properties`);
  }
  const actionProp = properties.action;
  if (!isRecord(actionProp)) {
    throw new ExtractionError(`tool ${def.name} has no action property`);
  }
  const enumValues = actionProp.enum;
  if (!Array.isArray(enumValues)) {
    throw new ExtractionError(`tool ${def.name} action enum is not an array`);
  }
  const actions: string[] = [];
  for (const value of enumValues) {
    if (typeof value !== 'string') {
      throw new ExtractionError(`tool ${def.name} action enum has non-string entry`);
    }
    actions.push(value);
  }
  return actions;
}

export interface RawOccurrence {
  readonly tool: string;
  readonly action: string;
  readonly evidence: Evidence;
}

/** Extract every {tool,action} occurrence from the authoritative definitions. */
export function extractOccurrences(): readonly RawOccurrence[] {
  const out: RawOccurrence[] = [];
  for (const def of allToolDefinitions) {
    const src = TOOL_SOURCE[def.name];
    if (src === undefined) {
      throw new ExtractionError(`no source mapping for tool ${def.name}`);
    }
    const seen = new Set<string>();
    for (const action of readActionEnum(def)) {
      if (seen.has(action)) {
        throw new ExtractionError(
          `tool ${def.name} declares duplicate action "${action}" in its enum`,
        );
      }
      seen.add(action);
      out.push({
        tool: def.name,
        action,
        evidence: { source: src.source, symbol: src.symbol, tool: def.name },
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
