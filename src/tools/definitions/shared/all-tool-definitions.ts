import { manageToolsToolDefinition } from '../core/manage-tools-tool.js';
import { manageAssetToolDefinition } from '../core/manage-asset-tool.js';
import { manageBlueprintToolDefinition } from '../core/blueprint/manage-blueprint-tool.js';
import { controlActorToolDefinition } from '../core/control-actor-tool.js';
import { controlEditorToolDefinition } from '../core/control-editor-tool.js';
import { manageLevelToolDefinition } from '../world/manage-level-tool.js';
import { buildEnvironmentToolDefinition } from '../world/build-environment-tool.js';
import { animationPhysicsToolDefinition } from '../gameplay/animation-physics-tool.js';
import { systemControlToolDefinition } from '../core/system-control-tool.js';
import { manageSequenceToolDefinition } from '../utility/manage-sequence-tool.js';
import { inspectToolDefinition } from '../core/inspect-tool.js';
import { manageAudioToolDefinition } from '../utility/manage-audio-tool.js';
import { manageGeometryToolDefinition } from '../world/manage-geometry-tool.js';
import { managePcgToolDefinition } from '../world/manage-pcg-tool.js';
import { manageEffectToolDefinition } from '../utility/manage-effect-tool.js';
import { manageGasToolDefinition } from '../gameplay/manage-gas-tool.js';
import { manageCharacterToolDefinition } from '../gameplay/manage-character-tool.js';
import { manageCombatToolDefinition } from '../gameplay/manage-combat-tool.js';
import { manageAiToolDefinition } from '../gameplay/ai/manage-ai-tool.js';
import { manageInventoryToolDefinition } from '../gameplay/manage-inventory-tool.js';
import { manageInteractionToolDefinition } from '../gameplay/manage-interaction-tool.js';
import { manageNetworkingToolDefinition } from '../utility/networking/manage-networking-tool.js';
import { manageLevelStructureToolDefinition } from '../world/manage-level-structure-tool.js';
// Custom authoring tools (ported)
import { manageDataTableToolDefinition } from '../core/manage-data-table-tool.js';
import { manageGameplayTagsToolDefinition } from '../core/manage-gameplay-tags-tool.js';
import { manageDataAssetToolDefinition } from '../core/manage-data-asset-tool.js';
import { manageLayersToolDefinition } from '../core/manage-layers-tool.js';
import { manageStringTableToolDefinition } from '../core/manage-string-table-tool.js';
import { manageAnimNotifyToolDefinition } from '../gameplay/manage-anim-notify-tool.js';
import { manageBlueprintInterfaceToolDefinition } from '../core/manage-blueprint-interface-tool.js';
import { managePhysicsMaterialToolDefinition } from '../gameplay/manage-physics-material-tool.js';
import type { ToolDefinition } from './tool-definition.js';

export const allToolDefinitions: ToolDefinition[] = [
  manageToolsToolDefinition,
  manageAssetToolDefinition,
  manageBlueprintToolDefinition,
  controlActorToolDefinition,
  controlEditorToolDefinition,
  manageLevelToolDefinition,
  buildEnvironmentToolDefinition,
  animationPhysicsToolDefinition,
  systemControlToolDefinition,
  manageSequenceToolDefinition,
  inspectToolDefinition,
  manageAudioToolDefinition,
  manageGeometryToolDefinition,
  managePcgToolDefinition,
  manageEffectToolDefinition,
  manageGasToolDefinition,
  manageCharacterToolDefinition,
  manageCombatToolDefinition,
  manageAiToolDefinition,
  manageInventoryToolDefinition,
  manageInteractionToolDefinition,
  manageNetworkingToolDefinition,
  manageLevelStructureToolDefinition,
  manageDataTableToolDefinition,
  manageGameplayTagsToolDefinition,
  manageDataAssetToolDefinition,
  manageLayersToolDefinition,
  manageStringTableToolDefinition,
  manageAnimNotifyToolDefinition,
  manageBlueprintInterfaceToolDefinition,
  managePhysicsMaterialToolDefinition
];
