// Task 27 — deterministic valid/invalid case generation from canonical records.
//
// One minimal VALID request per capability, plus a rule-invalid variant for every
// rule the capability's own schema actually declares. Nothing is invented: if a
// record declares no enum, it contributes no enum case.

import type { CapabilityLike } from './execute-reference.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

export const EXECUTE_RULES = [
  'valid',
  'undeclared-param',
  'missing-required',
  'required-one-of',
  'wrong-type',
  'enum',
  'range',
  'action-override',
  'gateway-control-in-params',
  'unsupported-option',
  'output-mismatch',
  'disabled-capability'
] as const;

export type ExecuteRule = (typeof EXECUTE_RULES)[number];

export type ExecuteCase = {
  readonly caseId: string;
  readonly rule: ExecuteRule;
  readonly capabilityId: string;
  readonly params: Record<string, unknown>;
  readonly options?: Record<string, unknown>;
  readonly toolEnabled: boolean;
  readonly dispatchOutput?: unknown;
  readonly expect: {
    readonly status: 'success' | 'error';
    readonly kind?: string;
    readonly gatewayCode?: string;
  };
};

type JsonRecord = Record<string, unknown>;

const propertiesOf = (schema: unknown): JsonRecord =>
  isRecord(schema) && isRecord(schema.properties) ? schema.properties : {};

const requiredOf = (schema: unknown): readonly string[] =>
  isRecord(schema) && Array.isArray(schema.required)
    ? schema.required.filter((name): name is string => typeof name === 'string' && name !== 'action')
    : [];

const requiredOneOfOf = (schema: unknown): readonly string[] =>
  isRecord(schema) && Array.isArray(schema.requiredOneOf)
    ? schema.requiredOneOf.filter((name): name is string => typeof name === 'string')
    : [];

const firstType = (propertySchema: unknown): string | undefined => {
  if (!isRecord(propertySchema)) return undefined;
  if (typeof propertySchema.type === 'string') return propertySchema.type;
  if (Array.isArray(propertySchema.type) && typeof propertySchema.type[0] === 'string') {
    return propertySchema.type[0];
  }
  return undefined;
};

// A schema-conformant sample value, preferring a declared enum/default so the
// generated valid case cannot trip an unrelated rule.
export function sampleValue(propertySchema: unknown): unknown {
  if (!isRecord(propertySchema)) return 'sample';
  if ('default' in propertySchema) return propertySchema.default;
  if (Array.isArray(propertySchema.enum) && propertySchema.enum.length > 0) return propertySchema.enum[0];
  switch (firstType(propertySchema)) {
    case 'string': {
      const max = typeof propertySchema.maxLength === 'number' ? propertySchema.maxLength : undefined;
      const base = '/Game/Task27/Sample';
      return max !== undefined && max < base.length ? base.slice(0, max) : base;
    }
    case 'number':
    case 'integer': {
      const min = typeof propertySchema.minimum === 'number' ? propertySchema.minimum : 0;
      const max = typeof propertySchema.maximum === 'number' ? propertySchema.maximum : min + 1;
      return Math.min(Math.max(min, 1), max);
    }
    case 'boolean':
      return true;
    case 'array': {
      const minItems = typeof propertySchema.minItems === 'number' ? propertySchema.minItems : 0;
      const item = sampleValue(propertySchema.items);
      return Array.from({ length: Math.max(minItems, 1) }, () => item);
    }
    case 'object': {
      const nested: JsonRecord = {};
      for (const name of requiredOf(propertySchema)) {
        nested[name] = sampleValue(propertiesOf(propertySchema)[name]);
      }
      return nested;
    }
    case 'null':
      return null;
    default:
      return 'sample';
  }
}

export function minimalValidParams(record: CapabilityLike): JsonRecord {
  const properties = propertiesOf(record.schemas.input);
  const params: JsonRecord = {};
  for (const name of requiredOf(record.schemas.input)) {
    params[name] = sampleValue(properties[name]);
  }
  // `requiredOneOf` means at least one of the listed properties must be present,
  // so a valid request must carry one; otherwise the valid case would fail.
  const group = requiredOneOfOf(record.schemas.input);
  if (group.length > 0) {
    const first = group[0] as string;
    params[first] = sampleValue(properties[first]);
  }
  return params;
}

const wrongTypedValue = (declared: string | undefined): unknown =>
  declared === 'string' ? 12345 : declared === 'array' ? 'not-an-array' : 'not-the-declared-type';

function findProperty(
  record: CapabilityLike,
  predicate: (propertySchema: JsonRecord) => boolean
): [string, JsonRecord] | undefined {
  for (const [name, propertySchema] of Object.entries(propertiesOf(record.schemas.input))) {
    if (name === 'action') continue;
    if (isRecord(propertySchema) && predicate(propertySchema)) return [name, propertySchema];
  }
  return undefined;
}

function outOfRangeValue(propertySchema: JsonRecord): unknown | undefined {
  if (typeof propertySchema.maximum === 'number') return propertySchema.maximum + 1;
  if (typeof propertySchema.minimum === 'number') return propertySchema.minimum - 1;
  if (typeof propertySchema.maxLength === 'number') return 'x'.repeat(propertySchema.maxLength + 1);
  if (typeof propertySchema.maxItems === 'number') {
    return Array.from({ length: propertySchema.maxItems + 1 }, () => sampleValue(propertySchema.items));
  }
  return undefined;
}

export function buildCasesForRecord(record: CapabilityLike): readonly ExecuteCase[] {
  const valid = minimalValidParams(record);
  const cases: ExecuteCase[] = [
    {
      caseId: `${record.id}#valid`,
      rule: 'valid',
      capabilityId: record.id,
      params: valid,
      toolEnabled: true,
      expect: { status: 'success' }
    },
    {
      caseId: `${record.id}#undeclared-param`,
      rule: 'undeclared-param',
      capabilityId: record.id,
      params: { ...valid, task27UndeclaredParameter: true },
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'UNDECLARED_PARAMETER' }
    },
    {
      caseId: `${record.id}#action-override`,
      rule: 'action-override',
      capabilityId: record.id,
      params: { ...valid, action: 'task27_override' },
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'INVALID_PARAMS' }
    },
    {
      caseId: `${record.id}#gateway-control-in-params`,
      rule: 'gateway-control-in-params',
      capabilityId: record.id,
      params: { ...valid, timeoutMs: 1000 },
      toolEnabled: true,
      expect: { status: 'error', kind: 'option', gatewayCode: 'UNSUPPORTED_OPTION' }
    },
    {
      caseId: `${record.id}#unsupported-option`,
      rule: 'unsupported-option',
      capabilityId: record.id,
      params: valid,
      options: { task27NotAnOption: true },
      toolEnabled: true,
      expect: { status: 'error', kind: 'option', gatewayCode: 'UNSUPPORTED_OPTION' }
    },
    {
      caseId: `${record.id}#disabled-capability`,
      rule: 'disabled-capability',
      capabilityId: record.id,
      params: valid,
      toolEnabled: false,
      expect: { status: 'error', kind: 'execution', gatewayCode: 'TOOL_DISABLED' }
    },
    {
      caseId: `${record.id}#output-mismatch`,
      rule: 'output-mismatch',
      capabilityId: record.id,
      params: valid,
      toolEnabled: true,
      dispatchOutput: { task27UndeclaredOutputField: true },
      expect: { status: 'error', kind: 'validation', gatewayCode: 'OUTPUT_SCHEMA_VIOLATION' }
    }
  ];

  const properties = propertiesOf(record.schemas.input);
  const required = requiredOf(record.schemas.input).filter((name) => {
    const propertySchema = properties[name];
    return !(isRecord(propertySchema) && 'default' in propertySchema);
  });
  if (required.length > 0) {
    const dropped = { ...valid };
    delete dropped[required[0] as string];
    cases.push({
      caseId: `${record.id}#missing-required`,
      rule: 'missing-required',
      capabilityId: record.id,
      params: dropped,
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'MISSING_REQUIRED_PARAMETER' }
    });
  }

  const group = requiredOneOfOf(record.schemas.input);
  if (group.length > 0) {
    const emptied = { ...valid };
    for (const name of group) delete emptied[name];
    cases.push({
      caseId: `${record.id}#required-one-of`,
      rule: 'required-one-of',
      capabilityId: record.id,
      params: emptied,
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'MISSING_REQUIRED_ONEOF' }
    });
  }

  const typed = findProperty(record, (schema) => typeof schema.type === 'string' && !Array.isArray(schema.enum));
  if (typed) {
    cases.push({
      caseId: `${record.id}#wrong-type`,
      rule: 'wrong-type',
      capabilityId: record.id,
      params: { ...valid, [typed[0]]: wrongTypedValue(firstType(typed[1])) },
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'INVALID_PARAMETER_TYPE' }
    });
  }

  const enumProperty = findProperty(record, (schema) => Array.isArray(schema.enum) && schema.enum.length > 0);
  if (enumProperty) {
    cases.push({
      caseId: `${record.id}#enum`,
      rule: 'enum',
      capabilityId: record.id,
      params: { ...valid, [enumProperty[0]]: 'task27-not-an-enum-member' },
      toolEnabled: true,
      expect: { status: 'error', kind: 'validation', gatewayCode: 'INVALID_PARAMETER_VALUE' }
    });
  }

  const bounded = findProperty(record, (schema) =>
    ['minimum', 'maximum', 'maxLength', 'maxItems'].some((keyword) => typeof schema[keyword] === 'number'));
  if (bounded) {
    const violating = outOfRangeValue(bounded[1]);
    if (violating !== undefined) {
      cases.push({
        caseId: `${record.id}#range`,
        rule: 'range',
        capabilityId: record.id,
        params: { ...valid, [bounded[0]]: violating },
        toolEnabled: true,
        expect: { status: 'error', kind: 'range', gatewayCode: 'OUT_OF_RANGE' }
      });
    }
  }

  return cases;
}
