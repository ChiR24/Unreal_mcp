import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { Draft202012ObjectSchemaSchema } from '../../../src/tools/catalog/capabilities/json-schema.js';
import {
  applyDeclaredDefaults,
  validateAgainstCapabilitySchema,
} from '../../../src/server/gateway/gateway-execute-validate.js';
import { isRecord as isRecordObject } from '../../../src/utils/validation/type-guards.js';

const EXPECTED_RECORDS = 1380;
const EXPECTED_SCHEMAS = EXPECTED_RECORDS * 2;

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

describe('Task 29 - every schema compiles under the production validation boundary', () => {
  it('all 2,670 input and output schemas parse under the shipped Zod schema contract', () => {
    const failures: string[] = [];
    let compiled = 0;

    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      for (const side of ['input', 'output'] as const) {
        const parsed = Draft202012ObjectSchemaSchema.safeParse(plain(record.schemas[side]));
        if (!parsed.success) {
          const issue = parsed.error.issues[0];
          failures.push(
            `${id} pointer=/schemas/${side}/${(issue?.path ?? []).join('/')} ${issue?.message ?? 'invalid'}`,
          );
          continue;
        }
        compiled += 1;
      }
    }

    expect(failures, `schema compilation failures:\n${failures.slice(0, 10).join('\n')}`).toEqual([]);
    expect(compiled).toBe(EXPECTED_SCHEMAS);
  });

  it('every declared example input type-checks, including every required field', () => {
    const wrongType: string[] = [];
    const missingRequired: string[] = [];
    let clean = 0;

    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      const schema = plain(record.schemas.input);
      const raw = plain(record.examples[0]?.input ?? {});
      if (!isRecordObject(raw)) {
        wrongType.push(`${id} pointer=/examples/0/input is not an object`);
        continue;
      }

      const properties = isRecordObject(schema) ? schema.properties : undefined;
      const declaresAction = isRecordObject(properties) && Object.hasOwn(properties, 'action');
      const { action: _routedAction, ...stripped } = raw;
      const params = declaresAction ? raw : stripped;

      const violation = validateAgainstCapabilitySchema(applyDeclaredDefaults(params, schema), schema);
      if (violation === undefined) {
        clean += 1;
        continue;
      }
      if (violation.reason === 'missing-required') {
        missingRequired.push(`${id} pointer=/examples/0/input${violation.pointer}`);
        continue;
      }
      wrongType.push(`${id} pointer=/examples/0/input${violation.pointer} ${violation.reason}: ${violation.message}`);
    }

    expect(
      wrongType,
      `example inputs violating a declared type/enum/bound:\n${wrongType.slice(0, 10).join('\n')}`,
    ).toEqual([]);
    expect(
      missingRequired,
      `example inputs missing required fields:\n${missingRequired.slice(0, 10).join('\n')}`,
    ).toEqual([]);
    expect(clean).toBe(EXPECTED_RECORDS);
  });

  it('every input schema is a closed object so unknown params can never pass silently', () => {
    const open: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      if (record.schemas.input.additionalProperties !== false) {
        open.push(`${String(record.id)} pointer=/schemas/input/additionalProperties`);
      }
      if (record.schemas.output.additionalProperties !== false) {
        open.push(`${String(record.id)} pointer=/schemas/output/additionalProperties`);
      }
    }
    expect(open, `open schemas:\n${open.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('every declared required property exists in its own properties map', () => {
    const dangling: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      for (const side of ['input', 'output'] as const) {
        const schema = record.schemas[side];
        const properties = schema.properties;
        for (const [index, required] of schema.required.entries()) {
          if (!Object.hasOwn(properties, required)) {
            dangling.push(`${id} pointer=/schemas/${side}/required/${index} names absent property "${required}"`);
          }
        }
      }
    }
    expect(dangling, `dangling required entries:\n${dangling.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('declared enums, numeric bounds and defaults survive into the shipped schemas', () => {
    let enums = 0;
    let bounds = 0;
    let defaults = 0;

    const walk = (node: unknown): void => {
      if (Array.isArray(node)) {
        for (const child of node) walk(child);
        return;
      }
      if (!isRecordObject(node)) return;
      if (Array.isArray(node.enum)) enums += 1;
      if (node.minimum !== undefined || node.maximum !== undefined
        || node.minItems !== undefined || node.maxItems !== undefined) {
        bounds += 1;
      }
      if (Object.hasOwn(node, 'default')) defaults += 1;
      for (const value of Object.values(node)) walk(value);
    };

    for (const record of ALL_CAPABILITY_RECORDS) walk(plain(record.schemas));

    expect(enums, 'no enum constraint survived into the shipped schemas').toBeGreaterThan(0);
    expect(bounds, 'no numeric/array bound survived into the shipped schemas').toBeGreaterThan(0);
    expect(defaults + enums + bounds).toBeGreaterThan(0);
  });
});

describe('Task 29 - output-honesty contract is held at zero debt', () => {
  const plainValue = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

  it('no record declares a non-boolean `success` output', () => {
    const offenders: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const success = record.schemas.output.properties.success;
      if (isRecordObject(success) && success.type !== 'boolean') {
        offenders.push(`${String(record.id)} pointer=/schemas/output/properties/success type=${String(success.type)}`);
      }
    }
    expect(
      offenders,
      `non-boolean \`success\` offenders:\n${offenders.slice(0, 10).join('\n')}`,
    ).toEqual([]);
  });

  it('no required output property is absent from its own example', () => {
    const offenders: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const example = plainValue(record.examples[0]?.output ?? {});
      if (!isRecordObject(example)) continue;
      for (const required of record.schemas.output.required) {
        if (!Object.hasOwn(example, required)) {
          offenders.push(`${String(record.id)} pointer=/schemas/output/required -> "${required}"`);
        }
      }
    }
    expect(
      offenders,
      `required-but-unexampled offenders:\n${offenders.slice(0, 10).join('\n')}`,
    ).toEqual([]);
  });

  it('no example output fails its own output schema', () => {
    const offenders: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const violation = validateAgainstCapabilitySchema(
        plainValue(record.examples[0]?.output ?? {}),
        plainValue(record.schemas.output),
      );
      if (violation !== undefined) {
        offenders.push(`${String(record.id)} pointer=/examples/0/output${violation.pointer} ${violation.reason}`);
      }
    }
    expect(
      offenders,
      `self-inconsistent example-output offenders:\n${offenders.slice(0, 10).join('\n')}`,
    ).toEqual([]);
  });

  it('no output property carries a degenerate description', () => {
    const offenders: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const properties = plainValue(record.schemas.output.properties);
      if (!isRecordObject(properties)) continue;
      for (const [name, node] of Object.entries(properties)) {
        if (isRecordObject(node) && node.description === name) {
          offenders.push(`${String(record.id)} pointer=/schemas/output/properties/${name}`);
        }
      }
    }
    expect(
      offenders,
      `degenerate output description offenders:\n${offenders.slice(0, 10).join('\n')}`,
    ).toEqual([]);
  });
});
