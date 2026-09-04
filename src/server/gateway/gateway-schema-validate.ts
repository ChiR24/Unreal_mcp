// src/server/gateway/gateway-schema-validate.ts
// Stage 4 schema validation for the canonical execute pipeline: the
// Draft-2020-12 keyword subset the generated capability records actually use,
// plus declared-default application. The same subset is implemented by the
// native `/mcp` validator and specified in
// `tests/unit/gateway-discovery-suite/{schema-subset,execute-reference}.ts`.
// Any keyword outside the supported set is rejected fail-closed on both
// surfaces. Extracted from gateway-execute-validate.ts.

import { hasOwn, isRecord } from '../../utils/validation/type-guards.js';

const SUPPORTED_SCHEMA_KEYWORDS = new Set([
  '$schema',
  'type',
  'description',
  'properties',
  'required',
  'requiredOneOf',
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

export type ViolationReason =
  | 'missing-required'
  | 'required-one-of'
  | 'undeclared'
  | 'type'
  | 'enum'
  | 'range'
  | 'unsupported-keyword';

/** Named per rule so the two surfaces emit the same precise code, not a catch-all. */
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
// (Implementation shared from src/utils/validation/type-guards.ts.)

function ownProperty(properties: Record<string, unknown>, key: string): unknown {
  return hasOwn(properties, key) ? properties[key] : undefined;
}

function validateObject(
  value: Record<string, unknown>,
  schema: Record<string, unknown>,
  pointer: string
): SchemaViolation | undefined {
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

  // At-least-one-of: mirrors CheckRequiredOneOf in McpNativeGatewaySchemaKeywords.cpp.
  if (Array.isArray(schema.requiredOneOf)) {
    const group = schema.requiredOneOf.filter((name): name is string => typeof name === 'string');
    if (group.length > 0 && !group.some((name) => hasOwn(value, name))) {
      return {
        reason: 'required-one-of',
        pointer: `${pointer}/requiredOneOf`,
        message: `At least one of [${group.join(', ')}] must be provided`
      };
    }
  }

  // An intentionally open object whose interior is arbitrary Unreal property data.
  // Required/requiredOneOf above still apply to its declared fields; only the
  // undeclared-key gate and property recursion are skipped for the interior.
  if (schema[REFLECTION_BOUNDARY_KEYWORD] === true) return undefined;

  // `additionalProperties: false` admits only declared keys. With no `properties`
  // declared at all, that means NO key is admitted — absent `properties` must not
  // read as "everything allowed", or the fail-closed gate would admit any key.
  if (schema.additionalProperties === false) {
    for (const key of Object.keys(value)) {
      if (properties === undefined || !hasOwn(properties, key)) {
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

// Vector-shaped parameters are declared as arrays on some parents (control_actor.spawn,
// manage_effect.spawn_niagara) and as {x,y,z} / {width,height} objects on others
// (manage_geometry.create_box, manage_audio.create_ambient_sound). Every handler accepts
// both spellings, so the gateway converts between them before validation instead of
// refusing the other shape with INVALID_PARAMETER_TYPE (dogfood #226).
//
// Matching is strict: a candidate key set only wins when the caller's OWN keys are a
// subset of the set and every set member that would be emitted is a finite number.
// Anything else (a typo'd key, a short or over-long array, a non-numeric component)
// falls through to the schema's guided type error instead of being silently truncated
// or having keys dropped. In a set of four keys the last one (w / a) is optional, so
// three of four are required. The sets and their order are mirrored by
// McpNativeGatewayVectorCoercion.cpp and pinned by
// tests/unit/tools/vector-shape-coercion-parity.test.ts — change both or neither.
export const VECTOR_KEY_SETS: ReadonlyArray<readonly string[]> = [
  ['x', 'y', 'z', 'w'],
  ['x', 'y', 'z'],
  ['pitch', 'yaw', 'roll'],
  ['r', 'g', 'b', 'a'],
  ['width', 'height'],
  ['x', 'y']
];

const isFiniteNumber = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);

function objectToVector(value: Record<string, unknown>): number[] | undefined {
  const ownKeys = Object.keys(value);
  for (const keys of VECTOR_KEY_SETS) {
    const required = keys.length === 4 ? keys.length - 1 : keys.length;
    if (!ownKeys.every((key) => keys.includes(key))) continue;
    if (!keys.slice(0, required).every((key) => hasOwn(value, key) && isFiniteNumber(value[key]))) continue;
    if (!ownKeys.every((key) => isFiniteNumber(value[key]))) continue;
    const vector: number[] = [];
    for (const key of keys) {
      const member = value[key];
      if (isFiniteNumber(member)) vector.push(member);
    }
    return vector;
  }
  return undefined;
}

function vectorToObject(values: readonly number[], propertySchema: Record<string, unknown>): Record<string, number> | undefined {
  const declared = isRecord(propertySchema.properties) ? Object.keys(propertySchema.properties) : [];
  if (declared.length === 0) return undefined;
  for (const keys of VECTOR_KEY_SETS) {
    const required = keys.length === 4 ? keys.length - 1 : keys.length;
    if (!keys.slice(0, required).every((key) => declared.includes(key))) continue;
    const consumed = keys.filter((key) => declared.includes(key));
    // Over-long arrays would silently drop their tail; short arrays below the
    // required prefix would fabricate handler defaults. Both refuse instead.
    if (values.length < required || values.length > consumed.length) continue;
    const out: Record<string, number> = {};
    for (let index = 0; index < values.length; index += 1) out[keys[index]] = values[index];
    return out;
  }
  return undefined;
}

export function coerceVectorShapes(args: Record<string, unknown>, schema: unknown): Record<string, unknown> {
  if (!isRecord(schema) || !isRecord(schema.properties)) return args;
  let out: Record<string, unknown> | undefined;
  for (const [name, propertySchema] of Object.entries(schema.properties)) {
    if (!isRecord(propertySchema) || !hasOwn(args, name)) continue;
    const value = args[name];
    const types = declaredTypes(propertySchema);
    let replacement: unknown;
    if (types.includes('array') && !types.includes('object') && isRecord(value) && !Array.isArray(value)) {
      replacement = objectToVector(value);
    } else if (types.includes('object') && !types.includes('array') && Array.isArray(value) && value.every(isFiniteNumber)) {
      replacement = vectorToObject(value, propertySchema);
    }
    if (replacement !== undefined) {
      out = out ?? { ...args };
      out[name] = replacement;
    }
  }
  return out ?? args;
}
