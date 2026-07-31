/**
 * tests/unit/capability-records/utility-wire-contract.test.ts
 *
 * TASK 29 GATE - zero tolerance. The canonical output schema of every utility
 * `*Info` capability must name and type the fields the plugin ACTUALLY emits.
 *
 * `utility-contract-honesty.test.ts` proves each record is consistent
 * with ITSELF (its example validates against its own schema). That is necessary
 * but not sufficient: a record can declare `frameworkInfo`, illustrate
 * `frameworkInfo`, validate perfectly, and still describe a key no transport
 * ever sends. This gate closes that gap by comparing the record against the
 * shipping C++ instead of against itself.
 *
 * Ground truth and its provenance live in `./utility-wire-observations.ts`;
 * every claim there is re-verified against the plugin sources below, so this
 * gate fails if either side moves.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { MANAGE_AUDIO_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-audio/index.js';
import { MANAGE_NETWORKING_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-networking/index.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-execute-validate.js';
import {
  CONTROL_CAPABILITY_ID,
  ENVELOPE_FIELDS,
  WIRE_OBSERVATIONS,
  provenanceFragment,
  type WireObservation,
} from './utility-wire-observations.js';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const RECORDS = [...MANAGE_AUDIO_RECORDS, ...MANAGE_NETWORKING_RECORDS];

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

const isRecordObject = (v: unknown): v is Record<string, unknown> =>
  typeof v === 'object' && v !== null && !Array.isArray(v);

function readPluginSource(relativePath: string): string {
  return readFileSync(resolve(REPO_ROOT, relativePath), 'utf8');
}

function recordFor(observation: WireObservation) {
  const found = RECORDS.find((r) => String(r.id) === observation.capabilityId);
  if (found === undefined) {
    throw new Error(`No utility record with id ${observation.capabilityId}`);
  }
  return found;
}

/** Declared output property names minus the envelope, i.e. the domain payload. */
function declaredDomainOutputs(observation: WireObservation): readonly string[] {
  const properties = recordFor(observation).schemas.output.properties;
  return Object.keys(properties).filter((name) => !ENVELOPE_FIELDS.has(name));
}

function declaredRequiredDomainOutputs(observation: WireObservation): readonly string[] {
  const required = recordFor(observation).schemas.output.required;
  return [...required].filter((name) => !ENVELOPE_FIELDS.has(name));
}

const jsonTypeOf = (value: unknown): string => {
  if (Array.isArray(value)) return 'array';
  if (value === null) return 'null';
  return typeof value;
};

describe('Task 29 utility wire contract - the observations are real', () => {
  it('finds every observed key emitted by the exact setter the fixture claims', () => {
    const offenders: string[] = [];
    for (const observation of WIRE_OBSERVATIONS) {
      for (const wire of observation.fields) {
        const fragment = provenanceFragment(wire.key, wire.type);
        if (!readPluginSource(wire.sourceFile).includes(fragment)) {
          offenders.push(`${observation.action} expected \`${fragment}\` in ${wire.sourceFile}`);
        }
      }
    }
    expect(offenders, `unbacked wire claims:\n${offenders.join('\n')}`).toEqual([]);
  });

  it('finds `success` written INTO the handler Result for every info handler (T29-B5)', () => {
    // T29-B5 was that `get_sessions_info` and `get_input_info` passed success
    // ONLY as the `SendAutomationResponse(..., true, ...)` envelope argument.
    // Native `BuildToolResult` publishes just the Result object, so `success`
    // never reached native `structuredContent` while every record requires it.
    const setter = `${provenanceFragment('success', 'boolean')}, true)`;
    const offenders = WIRE_OBSERVATIONS
      .filter((o) => !readPluginSource(o.successSourceFile).includes(setter))
      .map((o) => `${o.action} never writes \`${setter}\` into its Result (${o.successSourceFile})`);
    expect(offenders, `success absent from the native Result:\n${offenders.join('\n')}`).toEqual([]);
  });

  it('confirms each cited file really routes the action it is cited for', () => {
    const offenders = WIRE_OBSERVATIONS
      .filter((o) => !readPluginSource(o.routingFile).includes(`TEXT("${o.action}")`))
      .map((o) => `${o.action} is not routed in ${o.routingFile}`);
    expect(offenders, `miscited handlers:\n${offenders.join('\n')}`).toEqual([]);
  });
});

describe('Task 29 utility wire contract - no record declares a key nobody sends', () => {
  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s declares only output properties the plugin emits',
    (_action, observation) => {
      const emitted = new Set(observation.fields.map((f) => f.key));
      const phantom = declaredDomainOutputs(observation)
        .filter((name) => !emitted.has(name))
        .map((name) => `${observation.capabilityId} pointer=/schemas/output/properties/${name} is never emitted`);
      expect(phantom, `phantom output properties:\n${phantom.join('\n')}`).toEqual([]);
    },
  );

  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s declares every field the plugin always emits, with the emitted JSON type',
    (_action, observation) => {
      const properties = recordFor(observation).schemas.output.properties;
      const offenders: string[] = [];
      for (const wire of observation.fields.filter((f) => f.always)) {
        const node = properties[wire.key];
        if (!isRecordObject(node)) {
          offenders.push(
            `${observation.capabilityId} pointer=/schemas/output/properties/${wire.key} is absent but always emitted`,
          );
          continue;
        }
        if (node.type !== wire.type) {
          offenders.push(
            `${observation.capabilityId} pointer=/schemas/output/properties/${wire.key}/type is `
            + `"${String(node.type)}", wire emits "${wire.type}"`,
          );
        }
      }
      expect(offenders, `undeclared or mistyped wire fields:\n${offenders.join('\n')}`).toEqual([]);
    },
  );
});

describe('Task 29 utility wire contract - requiredness tracks what is always emitted', () => {
  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s requires exactly the always-emitted domain fields',
    (_action, observation) => {
      const alwaysEmitted = observation.fields.filter((f) => f.always).map((f) => f.key).sort();
      expect([...declaredRequiredDomainOutputs(observation)].sort()).toEqual(alwaysEmitted);
    },
  );

  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s declares its conditional fields without requiring them',
    (_action, observation) => {
      const conditional = observation.fields.filter((f) => !f.always).map((f) => f.key);
      if (conditional.length === 0) return;
      const declared = new Set(declaredDomainOutputs(observation));
      const required = new Set(declaredRequiredDomainOutputs(observation));
      const offenders: string[] = [];
      for (const key of conditional) {
        if (!declared.has(key)) {
          offenders.push(
            `${observation.capabilityId} pointer=/schemas/output/properties/${key} is emitted on some paths but undeclared, `
            + 'and the schema is closed so a real response would be rejected',
          );
        }
        if (required.has(key)) {
          offenders.push(`${observation.capabilityId} pointer=/schemas/output/required requires conditional "${key}"`);
        }
      }
      expect(offenders, `conditional-field drift:\n${offenders.join('\n')}`).toEqual([]);
    },
  );
});

describe('Task 29 utility wire contract - the first example is a real response', () => {
  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s illustrates every required domain field with a value of the emitted type',
    (_action, observation) => {
      const record = recordFor(observation);
      const output = plain(record.examples[0]?.output ?? undefined);
      expect(isRecordObject(output), `${observation.capabilityId} has no first example output`).toBe(true);
      if (!isRecordObject(output)) return;

      const offenders: string[] = [];
      for (const wire of observation.fields.filter((f) => f.always)) {
        if (!Object.hasOwn(output, wire.key)) {
          offenders.push(`${observation.capabilityId} pointer=/examples/0/output/${wire.key} is missing`);
          continue;
        }
        const actual = jsonTypeOf(output[wire.key]);
        const expected = wire.type === 'number' ? 'number' : wire.type;
        if (actual !== expected) {
          offenders.push(
            `${observation.capabilityId} pointer=/examples/0/output/${wire.key} is ${actual}, wire emits ${expected}`,
          );
        }
      }
      expect(offenders, `example does not match the wire:\n${offenders.join('\n')}`).toEqual([]);
    },
  );

  it.each(WIRE_OBSERVATIONS.map((o) => [o.action, o] as const))(
    '%s still satisfies its own repaired output schema',
    (_action, observation) => {
      const record = recordFor(observation);
      const violation = validateAgainstCapabilitySchema(
        plain(record.examples[0]?.output ?? {}),
        plain(record.schemas.output),
      );
      expect(violation, `${observation.capabilityId} example output violates its schema`).toBeUndefined();
    },
  );
});

describe('Task 29 utility wire contract - networking is the untouched control', () => {
  it('keeps get_networking_info declaring exactly the networkingInfo object it always emitted', () => {
    const control = RECORDS.find((r) => String(r.id) === CONTROL_CAPABILITY_ID);
    expect(control).toBeDefined();
    if (control === undefined) return;

    const properties = control.schemas.output.properties;
    expect(Object.keys(properties).sort()).toEqual(['message', 'networkingInfo', 'success']);
    expect([...control.schemas.output.required].sort()).toEqual(['networkingInfo', 'success']);
    expect(isRecordObject(properties.networkingInfo) && properties.networkingInfo.type).toBe('object');
    expect(plain(control.examples[0]?.output)).toEqual({
      success: true,
      message: 'get_networking_info completed successfully.',
      networkingInfo: { role: 'ROLE_Authority', remoteRole: 'ROLE_SimulatedProxy', hasAuthority: true },
    });
  });
});
