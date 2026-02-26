/**
 * Central exports for utility modules.
 * 
 * @example
 * import { Logger, ResponseFactory, sanitizePath } from '../utils/index.js';
 */

// Command validation
export { CommandValidator } from './command-validator.js';

// Elicitation (MCP prompt helpers)
export type {
  ElicitSchema,
  ElicitOptions,
} from './elicitation.js';
export { createElicitationHelper } from './elicitation.js';

// Error handling
export { ErrorType, ErrorHandler } from './error-handler.js';

// INI file reading
export { readIniFile, getProjectSetting } from './ini-reader.js';

// Logging
export type { LogLevel } from './logger.js';
export { Logger } from './logger.js';

// Normalization helpers
export type {
  Vec3Obj,
  Rot3Obj,
  Vec3Tuple,
  Rot3Tuple,
} from './normalize.js';
export {
  toVec3Object,
  toRotObject,
  toVec3Tuple,
  toRotTuple,
  toFiniteNumber,
  normalizePartialVector,
  normalizeTransformInput,
} from './normalize.js';

// Path security
export { sanitizePath as sanitizePathSecure } from './path-security.js';

// Response factory
export { ResponseFactory } from './response-factory.js';

// Response validation
export { ResponseValidator, responseValidator } from './response-validator.js';

// Result helpers
export type { InterpretedStandardResult } from './result-helpers.js';
export {
  interpretStandardResult,
  cleanResultText,
  bestEffortInterpretedText,
  coerceString,
  coerceStringArray,
  coerceBoolean,
  coerceNumber,
  coerceVector3,
} from './result-helpers.js';

// Safe JSON handling
export { cleanObject as cleanObjectSafe } from './safe-json.js';

// Command queue
export type { CommandQueueItem } from './unreal-command-queue.js';
export { UnrealCommandQueue } from './unreal-command-queue.js';

// Validation utilities
export {
  sanitizeCommandArgument,
  sanitizeAssetName,
  sanitizePath,
  validatePathLength,
  validateAssetParams,
  ensureVector3,
  concurrencyDelay,
  ensureColorRGB,
  ensureRotation,
} from './validation.js';

// Action constants
export {
  TOOL_ACTIONS,
  ACTOR_ACTIONS,
  INPUT_ACTIONS,
  type ToolAction,
  type ActorAction,
  type InputAction,
} from './action-constants.js';

// Type coercion helpers
export {
  toNumber,
  toBoolean,
  toString,
  toVec3Array,
  toRotArray,
  toColor3,
  toLocationObj,
  toRotationObj,
  validateAudioParams,
  normalizeName,
} from './type-coercion.js';
