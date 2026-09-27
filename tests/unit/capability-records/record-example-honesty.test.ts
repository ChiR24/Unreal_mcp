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
import { applyDeclaredDefaults, validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';
import { isRecord as isRecordObject } from '../../../src/utils/validation/type-guards.js';

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

/** Render offenders as a stable, greppable message capped to a readable window. */
const report = (label: string, offenders: readonly string[]): string =>
  `${label}: ${offenders.length} offender(s)\n${offenders.slice(0, 15).join('\n')}`;

describe('Task 29 - every record in the catalog carries an honest example', () => {
  it('declares at least one example per record', () => {
    const offenders = ALL_CAPABILITY_RECORDS.filter((record) => record.examples.length === 0).map(
      (record) => `${String(record.id)} pointer=/examples has no example`,
    );

    expect(offenders, report('records without an authored example', offenders)).toEqual([]);
  });

  it('has a first example whose input satisfies its own input schema', () => {
    const offenders: string[] = [];

    for (const record of ALL_CAPABILITY_RECORDS) {
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

    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      const output = plain(record.examples[0]?.output ?? {});
      const violation = validateAgainstCapabilitySchema(output, plain(record.schemas.output));
      if (violation !== undefined) {
        offenders.push(`${id} pointer=/examples/0/output${violation.pointer} ${violation.reason}: ${violation.message}`);
      }
    }

    expect(offenders, report('example outputs failing their own schema', offenders)).toEqual([]);
  });
});
