// Task 27 — the exact Draft-2020-12 keyword subset the canonical capability
// records actually use, expressed once as an executable specification.
//
// The native `/mcp` validator (McpNativeGatewaySchemaValidation.cpp) implements
// the same rules over the same generated data. Anything outside this set is
// rejected fail-closed on BOTH surfaces, so a record that later grows an
// unimplemented keyword can never be silently under-validated.

import { isRecord } from '../../../src/utils/validation/type-guards.js';

export const SUPPORTED_SCHEMA_KEYWORDS = [
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
  'x-unreal-reflection-boundary',
  'requiredOneOf'
] as const;

export type SupportedSchemaKeyword = (typeof SUPPORTED_SCHEMA_KEYWORDS)[number];

// A reflection boundary is an intentionally open object (Task 2): its interior is
// arbitrary Unreal property data, so interior keys are NOT whitelisted. The
// boundary itself still has to be an object.
export const REFLECTION_BOUNDARY_KEYWORD = 'x-unreal-reflection-boundary';

// Named per rule so TS and native emit the same precise code, not one catch-all.
export const VIOLATION_REASONS = [
  'missing-required',
  'required-one-of',
  'undeclared',
  'type',
  'enum',
  'range',
  'unsupported-keyword'
] as const;

export type ViolationReason = (typeof VIOLATION_REASONS)[number];

export const VIOLATION_GATEWAY_CODES: Readonly<Record<ViolationReason, string>> = {
  'missing-required': 'MISSING_REQUIRED_PARAMETER',
  'required-one-of': 'MISSING_REQUIRED_ONEOF',
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

type JsonRecord = Record<string, unknown>;

const supported = new Set<string>(SUPPORTED_SCHEMA_KEYWORDS);

function typeMatches(value: unknown, declared: string): boolean {
  switch (declared) {
    case 'string':
      return typeof value === 'string';
    case 'number':
      return typeof value === 'number' && Number.isFinite(value);
    case 'integer':
      return typeof value === 'number' && Number.isInteger(value);
    case 'boolean':
      return typeof value === 'boolean';
    case 'array':
      return Array.isArray(value);
    case 'object':
      return isRecord(value);
    case 'null':
      return value === null;
    default:
      return false;
  }
}

function declaredTypes(schema: JsonRecord): readonly string[] {
  const declared = schema.type;
  if (typeof declared === 'string') return [declared];
  if (Array.isArray(declared)) return declared.filter((entry): entry is string => typeof entry === 'string');
  return [];
}

function rejectUnsupportedKeywords(schema: JsonRecord, pointer: string): SchemaViolation | undefined {
  for (const keyword of Object.keys(schema)) {
    if (supported.has(keyword)) continue;
    return {
      reason: 'unsupported-keyword',
      pointer,
      message: `Schema keyword '${keyword}' at ${pointer} is not implemented by the canonical validator`
    };
  }
  return undefined;
}

function validateScalarBounds(value: unknown, schema: JsonRecord, pointer: string): SchemaViolation | undefined {
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

export function validateAgainstSubset(
  value: unknown,
  schema: unknown,
  pointer = ''
): SchemaViolation | undefined {
  if (!isRecord(schema)) {
    return { reason: 'type', pointer, message: `Schema at ${pointer || '/'} is not an object` };
  }
  const unsupported = rejectUnsupportedKeywords(schema, pointer || '/');
  if (unsupported) return unsupported;

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
  if (bounds) return bounds;

  if (isRecord(value)) {
    const violation = validateObject(value, schema, pointer);
    if (violation) return violation;
  }

  if (Array.isArray(value) && schema.items !== undefined) {
    for (let index = 0; index < value.length; index += 1) {
      const violation = validateAgainstSubset(value[index], schema.items, `${pointer}/${index}`);
      if (violation) return violation;
    }
  }

  return undefined;
}

function validateObject(value: JsonRecord, schema: JsonRecord, pointer: string): SchemaViolation | undefined {
  // Reflection boundary: interior shape is Unreal property data by contract.
  if (schema[REFLECTION_BOUNDARY_KEYWORD] === true) return undefined;

  const properties = isRecord(schema.properties) ? schema.properties : undefined;

  if (Array.isArray(schema.required)) {
    for (const name of schema.required) {
      if (typeof name === 'string' && !(name in value)) {
        return {
          reason: 'missing-required',
          pointer: `${pointer}/${name}`,
          message: `Missing required parameter '${name}'`
        };
      }
    }
  }

  // At-least-one-of: mirrors CheckRequiredOneOf in McpNativeGatewaySchemaKeywords.cpp.
  if (Array.isArray(schema.requiredOneOf)) {
    const group = schema.requiredOneOf.filter((name): name is string => typeof name === 'string');
    if (group.length > 0 && !group.some((name) => name in value)) {
      return {
        reason: 'required-one-of',
        pointer: `${pointer}/requiredOneOf`,
        message: `At least one of [${group.join(', ')}] must be provided`
      };
    }
  }

  if (schema.additionalProperties === false && properties) {
    for (const key of Object.keys(value)) {
      if (!(key in properties)) {
        return {
          reason: 'undeclared',
          pointer: `${pointer}/${key}`,
          message: `Undeclared parameter '${key}'`
        };
      }
    }
  }

  if (properties) {
    for (const [key, entry] of Object.entries(value)) {
      const propertySchema = properties[key];
      if (propertySchema === undefined) continue;
      const violation = validateAgainstSubset(entry, propertySchema, `${pointer}/${key}`);
      if (violation) return violation;
    }
  }

  return undefined;
}

// Defaults are applied before validation so a record's declared `default` is the
// value that actually reaches Unreal (native does the same, in the same order).
export function applyDefaults(params: JsonRecord, schema: unknown): JsonRecord {
  if (!isRecord(schema) || !isRecord(schema.properties)) return params;
  const withDefaults: JsonRecord = { ...params };
  for (const [name, propertySchema] of Object.entries(schema.properties)) {
    if (name in withDefaults) continue;
    if (isRecord(propertySchema) && 'default' in propertySchema) {
      withDefaults[name] = propertySchema.default;
    }
  }
  return withDefaults;
}
