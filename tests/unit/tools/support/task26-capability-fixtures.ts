// Task 26 — fixture generation for canonical execute validation.
//
// Every fixture is derived from a record's own generated input schema, so the
// suites stay exhaustive across all 1,335 actions without a hand-maintained
// list that would rot the moment the catalog is regenerated.
//
// The expected error code of each invalid variant follows the validator's rule
// order (required -> undeclared -> per-property type/enum/range), which is why
// each variant starts from a minimal VALID payload and breaks exactly one rule.

import type { CapabilityRecord, Draft202012ObjectSchema } from '../../../../src/tools/catalog/capabilities/model.js';

type JsonRecord = Record<string, unknown>;

const isRecord = (value: unknown): value is JsonRecord =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

const REFLECTION_BOUNDARY = 'x-unreal-reflection-boundary';

export const UNDECLARED_PARAMETER_NAME = 'task26_undeclared_probe';

function declaredType(schema: JsonRecord): string | undefined {
  if (typeof schema.type === 'string') return schema.type;
  if (Array.isArray(schema.type) && typeof schema.type[0] === 'string') return schema.type[0];
  return undefined;
}

function boundedNumber(schema: JsonRecord, integer: boolean): number {
  const min = typeof schema.minimum === 'number' ? schema.minimum : undefined;
  const max = typeof schema.maximum === 'number' ? schema.maximum : undefined;
  const candidate = min ?? (max !== undefined && max < 1 ? max : 1);
  const clamped = max !== undefined && candidate > max ? max : candidate;
  return integer ? Math.round(clamped) : clamped;
}

function boundedString(schema: JsonRecord): string {
  const maxLength = typeof schema.maxLength === 'number' ? schema.maxLength : undefined;
  const value = 'x';
  return maxLength !== undefined && maxLength < value.length ? value.slice(0, maxLength) : value;
}

export function validValueFor(schema: unknown): unknown {
  if (!isRecord(schema)) return null;
  if (Array.isArray(schema.enum) && schema.enum.length > 0) return schema.enum[0];

  switch (declaredType(schema)) {
    case 'string': return boundedString(schema);
    case 'number': return boundedNumber(schema, false);
    case 'integer': return boundedNumber(schema, true);
    case 'boolean': return true;
    case 'null': return null;
    case 'array': {
      const count = typeof schema.minItems === 'number' ? schema.minItems : 0;
      return Array.from({ length: count }, () => validValueFor(schema.items));
    }
    case 'object': {
      if (schema[REFLECTION_BOUNDARY] === true) return {};
      return minimalValidObject(schema);
    }
    default: return null;
  }
}

function minimalValidObject(schema: JsonRecord): JsonRecord {
  const properties = isRecord(schema.properties) ? schema.properties : {};
  const required = Array.isArray(schema.required) ? schema.required : [];
  const out: JsonRecord = {};
  for (const name of required) {
    if (typeof name !== 'string' || name === 'action') continue;
    out[name] = validValueFor(properties[name]);
  }
  return out;
}

/** The smallest params payload that satisfies a capability's declared contract. */
export function minimalValidParams(record: CapabilityRecord): JsonRecord {
  return minimalValidObject(record.schemas.input);
}

/** Lets a fixture reach the output stage instead of failing on a stand-in payload. */
export function minimalValidOutput(record: CapabilityRecord): JsonRecord {
  return { success: true, ...minimalValidObject(record.schemas.output) };
}

function declaredProperties(schema: Draft202012ObjectSchema): ReadonlyArray<readonly [string, JsonRecord]> {
  if (!isRecord(schema.properties)) return [];
  const entries: Array<readonly [string, JsonRecord]> = [];
  for (const [name, entry] of Object.entries<unknown>(schema.properties)) {
    if (name !== 'action' && isRecord(entry)) entries.push([name, entry]);
  }
  return entries;
}

function requiredNames(schema: Draft202012ObjectSchema): readonly string[] {
  return Array.isArray(schema.required)
    ? schema.required.filter((name): name is string => typeof name === 'string' && name !== 'action')
    : [];
}

function wrongTypedValue(schema: JsonRecord): { readonly value: unknown } | undefined {
  switch (declaredType(schema)) {
    case 'string': return { value: 12_345 };
    case 'number':
    case 'integer': return { value: 'not-a-number' };
    case 'boolean': return { value: 'not-a-boolean' };
    case 'array': return { value: 'not-an-array' };
    case 'object': return { value: 'not-an-object' };
    default: return undefined;
  }
}

function outOfRangeValue(schema: JsonRecord): { readonly value: unknown } | undefined {
  if (typeof schema.maximum === 'number') return { value: schema.maximum + 1 };
  if (typeof schema.minimum === 'number') return { value: schema.minimum - 1 };
  if (typeof schema.maxLength === 'number') return { value: 'x'.repeat(schema.maxLength + 1) };
  if (typeof schema.maxItems === 'number') {
    return { value: Array.from({ length: schema.maxItems + 1 }, () => validValueFor(schema.items)) };
  }
  if (typeof schema.minItems === 'number' && schema.minItems > 0) {
    return { value: Array.from({ length: schema.minItems - 1 }, () => validValueFor(schema.items)) };
  }
  return undefined;
}

export type InvalidVariant = {
  readonly rule: 'undeclared' | 'missing-required' | 'type' | 'enum' | 'range';
  readonly expectedErrorCode: string;
  readonly params: JsonRecord;
  readonly offendingParam: string;
};

/** One rule-breaking payload per rule the record's schema can actually express. */
export function invalidVariants(record: CapabilityRecord): readonly InvalidVariant[] {
  const schema = record.schemas.input;
  const base = minimalValidParams(record);
  const properties = declaredProperties(schema);
  const variants: InvalidVariant[] = [];

  variants.push({
    rule: 'undeclared',
    expectedErrorCode: 'UNDECLARED_PARAMETER',
    params: { ...base, [UNDECLARED_PARAMETER_NAME]: true },
    offendingParam: UNDECLARED_PARAMETER_NAME
  });

  // Declared defaults are applied before validation on both surfaces, so a
  // required parameter that declares one can never be reported missing. Only a
  // required parameter without a default can express this rule.
  const declaresDefault = new Set(
    properties.filter(([, entry]) => 'default' in entry).map(([name]) => name)
  );
  const [firstRequired] = requiredNames(schema).filter((name) => !declaresDefault.has(name));
  if (firstRequired !== undefined) {
    const withoutRequired = { ...base };
    delete withoutRequired[firstRequired];
    variants.push({
      rule: 'missing-required',
      expectedErrorCode: 'MISSING_REQUIRED_PARAMETER',
      params: withoutRequired,
      offendingParam: firstRequired
    });
  }

  const typed = properties
    .map(([name, entry]) => ({ name, wrong: wrongTypedValue(entry) }))
    .find((candidate) => candidate.wrong !== undefined);
  if (typed?.wrong !== undefined) {
    variants.push({
      rule: 'type',
      expectedErrorCode: 'INVALID_PARAMETER_TYPE',
      params: { ...base, [typed.name]: typed.wrong.value },
      offendingParam: typed.name
    });
  }

  const enumerated = properties.find(([, entry]) =>
    Array.isArray(entry.enum) && entry.enum.length > 0 && declaredType(entry) === 'string');
  if (enumerated !== undefined) {
    variants.push({
      rule: 'enum',
      expectedErrorCode: 'INVALID_PARAMETER_VALUE',
      params: { ...base, [enumerated[0]]: 'task26_not_a_declared_enum_member' },
      offendingParam: enumerated[0]
    });
  }

  const ranged = properties
    .map(([name, entry]) => ({ name, out: outOfRangeValue(entry) }))
    .find((candidate) => candidate.out !== undefined);
  if (ranged?.out !== undefined) {
    variants.push({
      rule: 'range',
      expectedErrorCode: 'OUT_OF_RANGE',
      params: { ...base, [ranged.name]: ranged.out.value },
      offendingParam: ranged.name
    });
  }

  return variants;
}

export function firstRecordWithVariant(
  records: readonly CapabilityRecord[],
  rule: InvalidVariant['rule']
): { readonly record: CapabilityRecord; readonly variant: InvalidVariant } {
  for (const record of records) {
    const variant = invalidVariants(record).find((entry) => entry.rule === rule);
    if (variant !== undefined) return { record, variant };
  }
  throw new Error(`no generated capability expresses the '${rule}' rule`);
}
