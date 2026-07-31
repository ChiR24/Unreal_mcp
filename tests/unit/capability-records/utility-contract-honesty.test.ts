/**
 * tests/unit/capability-records/utility-contract-honesty.test.ts
 *
 * TASK 29 GATE - zero-tolerance output/input contract honesty for the 127
 * utility records built by `records/utility/helpers.ts`.
 *
 * The aggregate Task 29 gate (`schema-compilation.test.ts`) PINS this
 * debt at its historical magnitude so it cannot grow. This gate is the opposite
 * polarity: it demands the debt be ZERO within the utility lane, so the repair
 * cannot silently regress once made.
 *
 * Scope is deliberately narrow - `manage_audio` + `manage_networking` only -
 * so it never collides with the parallel manage-asset / migration / native
 * lanes that are repairing their own records against the same aggregate gate.
 *
 * Ground truth for the assertions below (verified against the shipping plugin):
 *   Private/Domains/Networking/...HandlersInfo.cpp
 *     -> ResultJson->SetBoolField(TEXT("success"), true)
 *     -> SetObjectField(TEXT("networkingInfo"), ...) with role/remoteRole/hasAuthority
 *   Private/Domains/AudioAuthoring/...HandlersInfo.cpp
 *     -> Response->SetBoolField(TEXT("success"), true)
 * `success` is a BOOLEAN on the wire on both surfaces. A record declaring it as
 * a string is a defect, not a contract choice.
 */
import { describe, expect, it } from 'vitest';
import { MANAGE_AUDIO_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-audio/index.js';
import { MANAGE_NETWORKING_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-networking/index.js';
import {
  applyDeclaredDefaults,
  validateAgainstCapabilitySchema,
} from '../../../src/server/gateway/gateway-execute-validate.js';

const UTILITY_RECORDS = [...MANAGE_AUDIO_RECORDS, ...MANAGE_NETWORKING_RECORDS];
const EXPECTED_UTILITY_RECORDS = 127;

/** Round-trip so branded/readonly values compare and validate as plain data. */
const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

const isRecordObject = (v: unknown): v is Record<string, unknown> =>
  typeof v === 'object' && v !== null && !Array.isArray(v);

/**
 * The exact wording the five already-correct builders use (see
 * `records/core/builder.ts#outputSchema`). Reusing it is what makes the utility
 * lane indistinguishable from the correct lanes downstream.
 */
const SUCCESS_DESCRIPTION = 'Whether the action succeeded.';
const MESSAGE_DESCRIPTION = 'Human-readable result message.';

describe('Task 29 utility lane - the record universe under test', () => {
  it('covers exactly the 127 utility records owned by the two utility parents', () => {
    expect(UTILITY_RECORDS.length).toBe(EXPECTED_UTILITY_RECORDS);
    const parents = [...new Set(UTILITY_RECORDS.map((r) => String(r.routing.parentTool)))].sort();
    expect(parents).toEqual(['manage_audio', 'manage_networking']);
  });
});

describe('Task 29 utility lane - the output envelope is honest', () => {
  it('declares `success` as a boolean in every single utility record', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      const success = record.schemas.output.properties.success;
      if (!isRecordObject(success)) {
        offenders.push(`${String(record.id)} pointer=/schemas/output/properties/success is absent`);
        continue;
      }
      if (success.type !== 'boolean') {
        offenders.push(
          `${String(record.id)} pointer=/schemas/output/properties/success/type is "${String(success.type)}", expected "boolean"`,
        );
      }
    }
    expect(offenders, `non-boolean success:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('reuses the established core-builder wording for `success` and `message`', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      const props = record.schemas.output.properties;
      const success = props.success;
      const message = props.message;

      if (isRecordObject(success) && success.description !== SUCCESS_DESCRIPTION) {
        offenders.push(
          `${String(record.id)} pointer=/schemas/output/properties/success/description is "${String(success.description)}"`,
        );
      }
      if (!isRecordObject(message)) {
        offenders.push(`${String(record.id)} pointer=/schemas/output/properties/message is absent`);
        continue;
      }
      if (message.type !== 'string') {
        offenders.push(
          `${String(record.id)} pointer=/schemas/output/properties/message/type is "${String(message.type)}"`,
        );
      }
      if (message.description !== MESSAGE_DESCRIPTION) {
        offenders.push(
          `${String(record.id)} pointer=/schemas/output/properties/message/description is "${String(message.description)}"`,
        );
      }
    }
    expect(offenders, `header wording drift:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('carries no output property whose description merely echoes its own name', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      for (const [name, node] of Object.entries(record.schemas.output.properties)) {
        if (isRecordObject(node) && node.description === name) {
          offenders.push(`${String(record.id)} pointer=/schemas/output/properties/${name}/description echoes "${name}"`);
        }
      }
    }
    expect(offenders, `degenerate output descriptions:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  /**
   * Guards the weaker repair of merely de-camel-casing the field name
   * ("nodeId" -> "Node Id"), which clears the echo check without telling a
   * caller anything. Real prose is a sentence: several words, ending in a stop.
   */
  it('describes every output property with real prose, not a de-camel-cased name', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      for (const [name, node] of Object.entries(record.schemas.output.properties)) {
        if (!isRecordObject(node)) continue;
        const description = node.description;
        if (typeof description !== 'string') {
          offenders.push(`${String(record.id)} pointer=/schemas/output/properties/${name}/description is not a string`);
          continue;
        }
        const words = description.trim().split(/\s+/u);
        if (words.length < 3 || !description.trim().endsWith('.')) {
          offenders.push(
            `${String(record.id)} pointer=/schemas/output/properties/${name}/description is not prose: "${description}"`,
          );
        }
      }
    }
    expect(offenders, `non-prose output descriptions:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });
});

describe('Task 29 utility lane - every declared example is real and self-consistent', () => {
  it('gives every utility record a first example input that satisfies its own input schema', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      const schema = plain(record.schemas.input);
      const input = plain(record.examples[0]?.input ?? undefined);
      if (!isRecordObject(input)) {
        offenders.push(`${String(record.id)} pointer=/examples/0/input is missing or not an object`);
        continue;
      }
      const violation = validateAgainstCapabilitySchema(applyDeclaredDefaults(input, schema), schema);
      if (violation !== undefined) {
        offenders.push(
          `${String(record.id)} pointer=/examples/0/input${violation.pointer} ${violation.reason}: ${violation.message}`,
        );
      }
    }
    expect(offenders, `invalid example inputs:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('gives every utility record a first example output that satisfies its own output schema', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      const schema = plain(record.schemas.output);
      const output = plain(record.examples[0]?.output ?? undefined);
      if (!isRecordObject(output)) {
        offenders.push(`${String(record.id)} pointer=/examples/0/output is missing or not an object`);
        continue;
      }
      const violation = validateAgainstCapabilitySchema(output, schema);
      if (violation !== undefined) {
        offenders.push(
          `${String(record.id)} pointer=/examples/0/output${violation.pointer} ${violation.reason}: ${violation.message}`,
        );
      }
    }
    expect(offenders, `invalid example outputs:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('never satisfies a required example field with an empty placeholder object', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      for (const side of ['input', 'output'] as const) {
        const example = plain(record.examples[0]?.[side] ?? {});
        if (!isRecordObject(example)) continue;
        for (const [name, value] of Object.entries(example)) {
          const empty = (isRecordObject(value) && Object.keys(value).length === 0)
            || (Array.isArray(value) && value.length === 0)
            || value === '';
          if (empty) {
            offenders.push(`${String(record.id)} pointer=/examples/0/${side}/${name} is an empty placeholder`);
          }
        }
      }
    }
    expect(offenders, `placeholder example values:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });
});

describe('Task 29 utility lane - the input schema is not collateral damage', () => {
  /**
   * The repair fixes the OUTPUT header. `success` must never leak into the
   * shared input name/type inference set, because that set is keyed on bare
   * field names and would silently retype any future input called `success`.
   */
  it('never introduces `success` as an input property on any utility record', () => {
    const offenders = UTILITY_RECORDS
      .filter((r) => Object.hasOwn(r.schemas.input.properties, 'success'))
      .map((r) => `${String(r.id)} pointer=/schemas/input/properties/success`);
    expect(offenders, `success leaked into inputs:\n${offenders.join('\n')}`).toEqual([]);
  });

  it('preserves the established input type inference for known boolean/number/object fields', () => {
    const expected: Readonly<Record<string, string>> = {
      enabled: 'boolean',
      reliable: 'boolean',
      voiceEnabled: 'boolean',
      pushToTalkEnabled: 'boolean',
      volume: 'number',
      pitch: 'number',
      controllerId: 'number',
      attenuationRadius: 'number',
      playerIndex: 'number',
      voiceSettings: 'object',
      location: 'object',
      states: 'array',
      assetPath: 'string',
      blueprintPath: 'string',
    };
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      for (const [name, node] of Object.entries(record.schemas.input.properties)) {
        const want = expected[name];
        if (want === undefined || !isRecordObject(node)) continue;
        if (node.type !== want) {
          offenders.push(
            `${String(record.id)} pointer=/schemas/input/properties/${name}/type is "${String(node.type)}", expected "${want}"`,
          );
        }
      }
    }
    expect(offenders, `input schema retyped:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('keeps every utility input and output schema closed', () => {
    const offenders: string[] = [];
    for (const record of UTILITY_RECORDS) {
      for (const side of ['input', 'output'] as const) {
        if (record.schemas[side].additionalProperties !== false) {
          offenders.push(`${String(record.id)} pointer=/schemas/${side}/additionalProperties`);
        }
      }
    }
    expect(offenders, `open schemas:\n${offenders.join('\n')}`).toEqual([]);
  });
});
