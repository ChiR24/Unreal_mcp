/**
 * tests/unit/capability-records/record-example-honesty.test.ts
 *
 * Zero-tolerance example-honesty gate across the whole catalog.
 *
 * A capability record's example is the only executable documentation a client
 * ever sees for an action. An absent example teaches nothing; a present example
 * that fails the record's OWN schema teaches something false. Both are contract
 * defects, and this gate holds the entire surface at zero across every parent.
 */

import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import {
  applyDeclaredDefaults,
  validateAgainstCapabilitySchema,
} from '../../../src/server/gateway/gateway-execute-validate.js';

const EXPECTED_RECORDS = 1335;
const EXPECTED_PARENTS = 23;

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

const isRecordObject = (v: unknown): v is Record<string, unknown> =>
  typeof v === 'object' && v !== null && !Array.isArray(v);

const IN_SCOPE = ALL_CAPABILITY_RECORDS;

/** Render offenders as a stable, greppable message capped to a readable window. */
const report = (label: string, offenders: readonly string[]): string =>
  `${label}: ${offenders.length} offender(s)\n${offenders.slice(0, 15).join('\n')}`;

describe('Task 29 - every record in the catalog carries an honest example', () => {
  it('covers every record across every parent in the catalog', () => {
    const parents = new Set(ALL_CAPABILITY_RECORDS.map((record) => String(record.routing.parentTool)));
    expect(ALL_CAPABILITY_RECORDS.length).toBe(EXPECTED_RECORDS);
    expect(IN_SCOPE.length).toBe(EXPECTED_RECORDS);
    expect(parents.size).toBe(EXPECTED_PARENTS);
  });

  it('declares at least one example per record', () => {
    const offenders = IN_SCOPE.filter((record) => record.examples.length === 0).map(
      (record) => `${String(record.id)} pointer=/examples has no example`,
    );

    expect(offenders, report('records without an authored example', offenders)).toEqual([]);
  });

  it('has a first example whose input satisfies its own input schema', () => {
    const offenders: string[] = [];

    for (const record of IN_SCOPE) {
      const id = String(record.id);
      const schema = plain(record.schemas.input);
      const raw = plain(record.examples[0]?.input ?? {});
      if (!isRecordObject(raw)) {
        offenders.push(`${id} pointer=/examples/0/input is not an object`);
        continue;
      }

      // The routed `action` is part of the call envelope, not always a declared
      // parameter; strip it only when the schema does not declare it itself.
      const properties = isRecordObject(schema) ? schema.properties : undefined;
      const declaresAction = isRecordObject(properties) && Object.hasOwn(properties, 'action');
      const { action: _routedAction, ...stripped } = raw;
      const params = declaresAction ? raw : stripped;

      const violation = validateAgainstCapabilitySchema(applyDeclaredDefaults(params, schema), schema);
      if (violation !== undefined) {
        offenders.push(`${id} pointer=/examples/0/input${violation.pointer} ${violation.reason}: ${violation.message}`);
      }
    }

    expect(offenders, report('example inputs failing their own schema', offenders)).toEqual([]);
  });

  it('has a first example whose output satisfies its own output schema', () => {
    const offenders: string[] = [];

    for (const record of IN_SCOPE) {
      const id = String(record.id);
      const output = plain(record.examples[0]?.output ?? {});
      const violation = validateAgainstCapabilitySchema(output, plain(record.schemas.output));
      if (violation !== undefined) {
        offenders.push(`${id} pointer=/examples/0/output${violation.pointer} ${violation.reason}: ${violation.message}`);
      }
    }

    expect(offenders, report('example outputs failing their own schema', offenders)).toEqual([]);
  });

  it('names every required output property in the first example', () => {
    const offenders: string[] = [];

    for (const record of IN_SCOPE) {
      const output = plain(record.examples[0]?.output ?? {});
      if (!isRecordObject(output)) continue;
      for (const required of record.schemas.output.required) {
        if (!Object.hasOwn(output, required)) {
          offenders.push(`${String(record.id)} pointer=/examples/0/output omits required "${required}"`);
        }
      }
    }

    expect(offenders, report('required output properties absent from the example', offenders)).toEqual([]);
  });
});
