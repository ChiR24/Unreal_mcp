/**
 * Central exports for utility modules.
 *
 * @example
 * import { Logger, ResponseFactory, sanitizePathSecure } from '../utils/index.js';
 */

// Command validation
export { CommandValidator } from './commands/command-validator.js';

// Elicitation (MCP prompt helpers)
export type {
  ElicitSchema,
  ElicitOptions,
} from './interaction/elicitation.js';
export { createElicitationHelper } from './interaction/elicitation.js';

// Error handling
export { ErrorType, ErrorHandler } from './responses/error-handler.js';

// INI file reading
export { readIniFile, getProjectSetting } from './config/ini-reader.js';

// Logging
export type { LogLevel } from './logging/logger.js';
export { Logger } from './logging/logger.js';

// Normalization helpers
export type {
  Vec3Obj,
  Rot3Obj,
  Vec3Tuple,
  Rot3Tuple,
} from './validation/normalize.js';
export {
  toVec3Object,
  toRotObject,
  toVec3Tuple,
  toRotTuple,
  toFiniteNumber,
  normalizePartialVector,
  normalizeTransformInput,
} from './validation/normalize.js';

// Path security
export { sanitizePath as sanitizePathSecure } from './paths/path-security.js';

// Response factory
export { ResponseFactory } from './responses/response-factory.js';

// Response validation
export { ResponseValidator, responseValidator } from './responses/response-validator.js';

// Result helpers
export type { InterpretedStandardResult } from './responses/result-helpers.js';
export {
  interpretStandardResult,
  cleanResultText,
  bestEffortInterpretedText,
  coerceString,
  coerceStringArray,
  coerceBoolean,
  coerceNumber,
  coerceVector3,
} from './responses/result-helpers.js';

// Safe JSON handling
export { cleanObject as cleanObjectSafe } from './serialization/safe-json.js';

// Command queue
export type { CommandQueueItem } from './commands/unreal-command-queue.js';
export { UnrealCommandQueue } from './commands/unreal-command-queue.js';

// Validation utilities
export {
  sanitizeCommandArgument,
  sanitizeAssetName,
  normalizeAndSanitizeAssetPath,
} from './validation/validation.js';

// Action constants
export {
  TOOL_ACTIONS,
  ACTOR_ACTIONS,
  INPUT_ACTIONS,
  type ToolAction,
  type ActorAction,
  type InputAction,
} from './commands/action-constants.js';

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
} from './validation/type-coercion.js';

// Type guards
export { isRecord } from './validation/type-guards.js';
