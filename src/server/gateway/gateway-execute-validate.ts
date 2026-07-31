// src/server/gateway/gateway-execute-validate.ts
// Stages 2-4 of the canonical execute pipeline: gateway options, declared
// defaults, and the exact per-action input/output schema.
//
// The schema rules implemented here are the Draft-2020-12 keyword subset the
// generated capability records actually use. The same subset is implemented by
// the native `/mcp` validator, and the shared executable specification lives in
// `tests/unit/gateway-discovery-suite/{schema-subset,execute-reference}.ts`. Any keyword
// outside the supported set is rejected fail-closed on both surfaces, so a
// record that later grows an unimplemented keyword can never be silently
// under-validated.

import { isRecord } from '../../utils/validation/type-guards.js';
import {
  EXECUTION_OPTION_KEYS,
  LIVE_STATE_REVISION_KEYS
} from '../../tools/catalog/capabilities/semantic/execution-options.js';

const SUPPORTED_SCHEMA_KEYWORDS = new Set([
  '$schema',
  'type',
  'description',
  'properties',
  'required',
  'additionalProperties',
  'items',
  'minItems',
  'maxItems',
  'enum',
  'default',
  'minimum',
  'maximum',
  'maxLength',
  'x-unreal-reflection-boundary'
]);

/** An intentionally open object whose interior is arbitrary Unreal property data. */
const REFLECTION_BOUNDARY_KEYWORD = 'x-unreal-reflection-boundary';

export const MAX_TIMEOUT_MS = 600_000;

export type ViolationReason =
  | 'missing-required'
  | 'undeclared'
  | 'type'
  | 'enum'
  | 'range'
  | 'unsupported-keyword';

/** Named per rule so the two surfaces emit the same precise code, not a catch-all. */
export const VIOLATION_GATEWAY_CODES: Readonly<Record<ViolationReason, string>> = {
  'missing-required': 'MISSING_REQUIRED_PARAMETER',
  undeclared: 'UNDECLARED_PARAMETER',
  type: 'INVALID_PARAMETER_TYPE',
  enum: 'INVALID_PARAMETER_VALUE',
  range: 'OUT_OF_RANGE',
  'unsupported-keyword': 'UNSUPPORTED_SCHEMA_KEYWORD'
};

export type SchemaViolation = {
  readonly reason: ViolationReason;
  readonly pointer: string;
  readonly message: string;
};

export type OptionViolation = {
  readonly errorCode: 'UNSUPPORTED_OPTION' | 'INVALID_OPTIONS' | 'OUT_OF_RANGE' | 'UNSUPPORTED_PREVIEW';
  readonly message: string;
  readonly option?: string;
  readonly pointer?: string;
};

/** The accepted options that a dispatch path actually reads. `preview` is not one. */
export const HONORED_EXECUTION_OPTION_KEYS: readonly string[] =
  EXECUTION_OPTION_KEYS.filter((key) => key !== 'preview');

function typeMatches(value: unknown, declared: string): boolean {
  switch (declared) {
    case 'string': return typeof value === 'string';
    case 'number': return typeof value === 'number' && Number.isFinite(value);
    case 'integer': return typeof value === 'number' && Number.isInteger(value);
    case 'boolean': return typeof value === 'boolean';
    case 'array': return Array.isArray(value);
    case 'object': return isRecord(value);
    case 'null': return value === null;
    default: return false;
  }
}

function declaredTypes(schema: Record<string, unknown>): readonly string[] {
  const declared = schema.type;
  if (typeof declared === 'string') return [declared];
  if (Array.isArray(declared)) return declared.filter((entry): entry is string => typeof entry === 'string');
  return [];
}

function rejectUnsupportedKeywords(schema: Record<string, unknown>, pointer: string): SchemaViolation | undefined {
  for (const keyword of Object.keys(schema)) {
    if (SUPPORTED_SCHEMA_KEYWORDS.has(keyword)) continue;
    return {
      reason: 'unsupported-keyword',
      pointer,
      message: `Schema keyword '${keyword}' at ${pointer} is not implemented by the canonical validator`
    };
  }
  return undefined;
}

function validateScalarBounds(
  value: unknown,
  schema: Record<string, unknown>,
  pointer: string
): SchemaViolation | undefined {
  if (typeof value === 'number') {
    if (typeof schema.minimum === 'number' && value < schema.minimum) {
      return { reason: 'range', pointer, message: `${pointer} must be >= ${schema.minimum}` };
    }
    if (typeof schema.maximum === 'number' && value > schema.maximum) {
      return { reason: 'range', pointer, message: `${pointer} must be <= ${schema.maximum}` };
    }
  }
  if (typeof value === 'string' && typeof schema.maxLength === 'number' && value.length > schema.maxLength) {
    return { reason: 'range', pointer, message: `${pointer} must be at most ${schema.maxLength} characters` };
  }
  if (Array.isArray(value)) {
    if (typeof schema.minItems === 'number' && value.length < schema.minItems) {
      return { reason: 'range', pointer, message: `${pointer} must have at least ${schema.minItems} item(s)` };
    }
    if (typeof schema.maxItems === 'number' && value.length > schema.maxItems) {
      return { reason: 'range', pointer, message: `${pointer} must have at most ${schema.maxItems} item(s)` };
    }
  }
  return undefined;
}

// `key in object` walks the prototype chain, so `__proto__`, `constructor`,
// `toString` and every other Object.prototype member would read as "declared"
// and slip past the additionalProperties gate into dispatch. Native compares
// against a TMap and has no such chain, so own-key lookup is also what keeps
// the two surfaces reporting the same code for the same payload.
export function hasOwn(target: object, key: string): boolean {
  return Object.prototype.hasOwnProperty.call(target, key);
}

function ownProperty(properties: Record<string, unknown>, key: string): unknown {
  return hasOwn(properties, key) ? properties[key] : undefined;
}

function validateObject(
  value: Record<string, unknown>,
  schema: Record<string, unknown>,
  pointer: string
): SchemaViolation | undefined {
  if (schema[REFLECTION_BOUNDARY_KEYWORD] === true) return undefined;

  const properties = isRecord(schema.properties) ? schema.properties : undefined;

  if (Array.isArray(schema.required)) {
    for (const name of schema.required) {
      if (typeof name === 'string' && !hasOwn(value, name)) {
        return {
          reason: 'missing-required',
          pointer: `${pointer}/${name}`,
          message: `Missing required parameter '${name}'`
        };
      }
    }
  }

  if (schema.additionalProperties === false && properties !== undefined) {
    for (const key of Object.keys(value)) {
      if (!hasOwn(properties, key)) {
        return {
          reason: 'undeclared',
          pointer: `${pointer}/${key}`,
          message: `Undeclared parameter '${key}'`
        };
      }
    }
  }

  if (properties !== undefined) {
    for (const [key, entry] of Object.entries(value)) {
      const propertySchema = ownProperty(properties, key);
      if (propertySchema === undefined) continue;
      const violation = validateAgainstCapabilitySchema(entry, propertySchema, `${pointer}/${key}`);
      if (violation !== undefined) return violation;
    }
  }

  return undefined;
}

export function validateAgainstCapabilitySchema(
  value: unknown,
  schema: unknown,
  pointer = ''
): SchemaViolation | undefined {
  if (!isRecord(schema)) {
    return { reason: 'type', pointer, message: `Schema at ${pointer || '/'} is not an object` };
  }
  const unsupported = rejectUnsupportedKeywords(schema, pointer || '/');
  if (unsupported !== undefined) return unsupported;

  const types = declaredTypes(schema);
  if (types.length > 0 && !types.some((declared) => typeMatches(value, declared))) {
    return {
      reason: 'type',
      pointer: pointer || '/',
      message: `${pointer || '/'} must be of type ${types.join(' | ')}`
    };
  }

  if (Array.isArray(schema.enum) && !schema.enum.some((allowed) => allowed === value)) {
    return {
      reason: 'enum',
      pointer: pointer || '/',
      message: `${pointer || '/'} must be one of [${schema.enum.map(String).join(', ')}]`
    };
  }

  const bounds = validateScalarBounds(value, schema, pointer || '/');
  if (bounds !== undefined) return bounds;

  if (isRecord(value)) {
    const violation = validateObject(value, schema, pointer);
    if (violation !== undefined) return violation;
  }

  if (Array.isArray(value) && schema.items !== undefined) {
    for (let index = 0; index < value.length; index += 1) {
      const violation = validateAgainstCapabilitySchema(value[index], schema.items, `${pointer}/${index}`);
      if (violation !== undefined) return violation;
    }
  }

  return undefined;
}

// Defaults are applied before validation so a record's declared `default` is the
// value that actually reaches Unreal. Native applies the same top-level rule.
export function applyDeclaredDefaults(
  params: Record<string, unknown>,
  schema: unknown
): Record<string, unknown> {
  if (!isRecord(schema) || !isRecord(schema.properties)) return params;
  const withDefaults: Record<string, unknown> = { ...params };
  for (const [name, propertySchema] of Object.entries(schema.properties)) {
    if (hasOwn(withDefaults, name)) continue;
    if (isRecord(propertySchema) && hasOwn(propertySchema, 'default')) {
      withDefaults[name] = propertySchema.default;
    }
  }
  return withDefaults;
}

/**
 * Cross-cutting execution controls live in the gateway `options` envelope and
 * never inside action `params`. The supported key set is Task 3's; the value
 * rules match the shared execute reference exactly so the two surfaces agree.
 */
export function validateExecutionOptions(raw: unknown): OptionViolation | undefined {
  if (raw === undefined || raw === null) return undefined;
  if (!isRecord(raw)) {
    return { errorCode: 'INVALID_OPTIONS', message: 'options must be an object.' };
  }

  const supported = new Set<string>(EXECUTION_OPTION_KEYS);
  for (const key of Object.keys(raw)) {
    if (!supported.has(key)) {
      return {
        errorCode: 'UNSUPPORTED_OPTION',
        option: key,
        message: `Unsupported execution option '${key}'. Supported: [${EXECUTION_OPTION_KEYS.join(', ')}]`
      };
    }
  }

  const timeout = raw.timeoutMs;
  if (timeout !== undefined
    && (typeof timeout !== 'number' || !Number.isInteger(timeout) || timeout <= 0 || timeout > MAX_TIMEOUT_MS)) {
    return {
      errorCode: 'OUT_OF_RANGE',
      option: 'timeoutMs',
      message: `options.timeoutMs must be an integer in 1..${MAX_TIMEOUT_MS}`
    };
  }

  const preview = raw.preview;
  if (preview !== undefined && typeof preview !== 'boolean') {
    return {
      errorCode: 'INVALID_OPTIONS',
      pointer: '/options/preview',
      message: 'options.preview must be a boolean.'
    };
  }

  return validateExpectedRevisions(raw.expectedRevisions);
}

/**
 * Shape-check the Task 42 live-state pins. Mirrors McpParseExpectedRevisions in
 * the plugin exactly, so both transports refuse the same input with the same
 * code. The revision COMPARISON is deliberately not done here: it belongs on the
 * game thread immediately before mutation, where the value cannot be stale yet.
 */
function validateExpectedRevisions(raw: unknown): OptionViolation | undefined {
  if (raw === undefined) return undefined;
  if (!isRecord(raw)) {
    return {
      errorCode: 'INVALID_OPTIONS',
      pointer: '/options/expectedRevisions',
      message: 'options.expectedRevisions must be an object of state revisions.'
    };
  }

  const pinnable: readonly string[] = LIVE_STATE_REVISION_KEYS;
  for (const [key, value] of Object.entries(raw)) {
    if (!pinnable.includes(key)) {
      return {
        errorCode: 'UNSUPPORTED_OPTION',
        option: `expectedRevisions.${key}`,
        message: `Unsupported expected revision '${key}'. Supported: [${pinnable.join(', ')}]`
      };
    }
    if (typeof value !== 'number' || !Number.isInteger(value) || value < 1) {
      return {
        errorCode: 'OUT_OF_RANGE',
        option: `expectedRevisions.${key}`,
        message: `options.expectedRevisions.${key} must be an integer >= 1`
      };
    }
  }

  return undefined;
}

/** A gateway control smuggled into action params is refused, never forwarded. */
export function findControlKeyInParams(params: Record<string, unknown>): string | undefined {
  return EXECUTION_OPTION_KEYS.find((control) => hasOwn(params, control));
}

/**
 * The single refusal text both transports emit, so a client sees the same
 * sentence whether it reached the gateway over stdio or native `/mcp`. The
 * native mirror interpolates the capability id through FString::Printf with the
 * identical wording (asserted by the Task 43 transport-equivalence suite).
 */
export function unsupportedPreviewMessage(capabilityId: string): string {
  return `Capability '${capabilityId}' does not implement options.preview. `
    + 'No dispatch path performs a dry run, so preview:true would perform the real operation. '
    + 'Re-send without options.preview to execute for real.';
}

/**
 * `preview: true` is refused for every capability, before dispatch.
 *
 * No dispatch path reads the option, so there is no dry run to perform: honoring
 * the request would apply the real, irreversible mutation and then report it as
 * a preview. `behavior.supportsPreview` is deliberately NOT consulted — 124
 * records declare it and 10 of those are destructive, yet not one declaration is
 * backed by an implementation, so trusting it would leave the fake dry run in
 * place for exactly the most dangerous capabilities.
 */
export function checkPreviewSupport(
  rawOptions: unknown,
  capabilityId: string
): OptionViolation | undefined {
  if (!isRecord(rawOptions) || rawOptions.preview !== true) return undefined;
  return {
    errorCode: 'UNSUPPORTED_PREVIEW',
    option: 'preview',
    pointer: '/options/preview',
    message: unsupportedPreviewMessage(capabilityId)
  };
}
