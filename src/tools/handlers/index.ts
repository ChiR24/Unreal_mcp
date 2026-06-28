/**
 * Central exports for tool handlers.
 *
 * @example
 * import { handleActorTools, handleAssetTools } from './handlers/index.js';
 */

// Core utilities
export {
  executeAutomationRequest,
  requireAction,
  requireNonEmptyString,
  validateSecurityPatterns,
  validateArgsSecurity,
  validateRequiredParams,
  validateExpectedParams,
  ensureArgsPresent,
  normalizeLocation,
  normalizeRotation,
} from './foundation/dispatch/common-handlers.js';

// Argument helpers
export type { ArgConfig, NormalizedArgs } from './foundation/arguments/argument-helper.js';
export {
  normalizeArgs,
  normalizeArgsTyped,
  extractString,
  extractOptionalString,
  extractNumber,
  extractOptionalNumber,
  extractBoolean,
  extractOptionalBoolean,
  extractArray,
  extractOptionalArray,
  extractOptionalObject,
  resolveObjectPath,
} from './foundation/arguments/argument-helper.js';

// Domain handlers
export { handleActorTools } from './actor/actor-handlers.js';
export { handleAITools } from './ai/ai-handlers.js';
export { handleAnimationAuthoringTools } from './animation/authoring/animation-authoring-handlers.js';
export { handleAnimationTools } from './animation/runtime/animation-handlers.js';
export { handleAssetTools } from './asset/asset-handlers.js';
export { handleAudioAuthoringTools } from './audio/authoring/audio-authoring-handlers.js';
export { handleAudioTools } from './audio/runtime/audio-handlers.js';
export { handleBlueprintTools, handleBlueprintGet } from './blueprint/blueprint-handlers.js';
export { handleCharacterTools } from './character/character-handlers.js';
export { handleCombatTools } from './combat/combat-handlers.js';
export { handleEditorTools } from './editor/editor-handlers.js';
export { handleEffectTools } from './effect/effect-handlers.js';
export { handleEnvironmentTools } from './environment/environment-handlers.js';
export { handleGameFrameworkTools } from './game-framework/game-framework-handlers.js';
export { handleGASTools } from './gas/gas-handlers.js';
export { handleGeometryTools } from './geometry/geometry-handlers.js';
export { handleGraphTools } from './graph/graph-handlers.js';
export { handleInputTools } from './input/input-handlers.js';
export { handleInspectTools } from './inspect/inspect-handlers.js';
export { handleInteractionTools } from './interaction/interaction-handlers.js';
export { handleInventoryTools } from './inventory/inventory-handlers.js';
export { handleLevelTools } from './level/runtime/level-handlers.js';
export { handleLevelStructureTools } from './level/structure/level-structure-handlers.js';
export { handleLightingTools } from './lighting/lighting-handlers.js';
export { handleManageToolsTools } from './tools/manage-tools-handlers.js';
export { handleMaterialAuthoringTools } from './material/material-authoring-handlers.js';
export { handleNavigationTools } from './navigation/navigation-handlers.js';
export { handleNetworkingTools } from './networking/networking-handlers.js';
export { handleNiagaraAuthoringTools } from './niagara/niagara-authoring-handlers.js';
export { handlePerformanceTools } from './performance/performance-handlers.js';
export { handlePipelineTools } from './pipeline/pipeline-handlers.js';
export { handleSequenceTools } from './sequence/sequence-handlers.js';
export { handleSessionsTools } from './sessions/sessions-handlers.js';
export { handleSkeletonTools } from './skeleton/skeleton-handlers.js';
export { handleSplineTools } from './spline/spline-handlers.js';
export { handleSystemTools } from './system/system-handlers.js';
export { handleTextureTools } from './texture/texture-handlers.js';
export { handleVolumeTools } from './volume/volume-handlers.js';
export { handleWidgetAuthoringTools } from './widget/widget-authoring-handlers.js';
// Custom authoring tools (ported)
export { handleDataTableTools } from './data-table/data-table-handlers.js';
export { handleGameplayTagTools } from './gameplay-tags/gameplay-tags-handlers.js';
export { handleDataAssetTools } from './data-asset/data-asset-handlers.js';
export { handleLayerTools } from './layer/layer-handlers.js';
