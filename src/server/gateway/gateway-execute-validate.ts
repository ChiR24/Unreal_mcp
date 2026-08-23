// src/server/gateway/gateway-execute-validate.ts
// Public facade for stages 2-4 of the canonical execute pipeline: gateway
// options, declared defaults, and the exact per-action input/output schema.
// Schema validation lives in gateway-schema-validate.ts and the option rules in
// gateway-option-validate.ts; this module preserves the original exports exactly
// so existing importers of `./gateway-execute-validate.js` are unchanged.

export {
  applyDeclaredDefaults,
  validateAgainstCapabilitySchema,
  VIOLATION_GATEWAY_CODES,
  type SchemaViolation,
  type ViolationReason,
} from './gateway-schema-validate.js';

export { hasOwn } from '../../utils/validation/type-guards.js';

export {
  checkPreviewSupport,
  findControlKeyInParams,
  HONORED_EXECUTION_OPTION_KEYS,
  MAX_TIMEOUT_MS,
  unimplementedOptionMessage,
  UNIMPLEMENTED_EXECUTION_OPTION_KEYS,
  unsupportedPreviewMessage,
  validateExecutionOptions,
  type OptionViolation,
} from './gateway-option-validate.js';
