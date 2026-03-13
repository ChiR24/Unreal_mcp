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
} from './common-handlers.js';

// Argument helpers
export type { ArgConfig, NormalizedArgs } from './argument-helper.js';
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
} from './argument-helper.js';

// Domain handlers
export { handleActorTools } from './actor-handlers.js';
export { handleAITools } from './ai-handlers.js';
export { handleAnimationAuthoringTools } from './animation-authoring-handlers.js';
export { handleAnimationTools } from './animation-handlers.js';
export { handleAssetTools } from './asset-handlers.js';
export { handleAudioAuthoringTools } from './audio-authoring-handlers.js';
export { handleAudioTools } from './audio-handlers.js';
export { handleBlueprintTools, handleBlueprintGet } from './blueprint-handlers.js';
export { handleCharacterTools } from './character-handlers.js';
export { handleCombatTools } from './combat-handlers.js';
export { handleEditorTools } from './editor-handlers.js';
export { handleEffectTools } from './effect-handlers.js';
export { handleEnvironmentTools } from './environment-handlers.js';
export { handleGameFrameworkTools } from './game-framework-handlers.js';
export { handleGASTools } from './gas-handlers.js';
export { handleGeometryTools } from './geometry-handlers.js';
export { handleGraphTools } from './graph-handlers.js';
export { handleInputTools } from './input-handlers.js';
export { handleInspectTools } from './inspect-handlers.js';
export { handleInteractionTools } from './interaction-handlers.js';
export { handleInventoryTools } from './inventory-handlers.js';
export { handleLevelTools } from './level-handlers.js';
export { handleLevelStructureTools } from './level-structure-handlers.js';
export { handleLightingTools } from './lighting-handlers.js';
export { handleManageToolsTools } from './manage-tools-handlers.js';
export { handleMaterialAuthoringTools } from './material-authoring-handlers.js';
export { handleNavigationTools } from './navigation-handlers.js';
export { handleNetworkingTools } from './networking-handlers.js';
export { handleNiagaraAuthoringTools } from './niagara-authoring-handlers.js';
export { handlePerformanceTools } from './performance-handlers.js';
export { handlePipelineTools } from './pipeline-handlers.js';
export { handleSequenceTools } from './sequence-handlers.js';
export { handleSessionsTools } from './sessions-handlers.js';
export { handleSkeletonTools } from './skeleton-handlers.js';
export { handleSplineTools } from './spline-handlers.js';
export { handleSystemTools } from './system-handlers.js';
export { handleTextureTools } from './texture-handlers.js';
export { handleVolumeTools } from './volume-handlers.js';
export { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';
