// Fold subsumption: the folded family schema must accept at least what every
// member accepted pre-fold. widen() keeps the wider of two definitions, so a
// member's example input and every value its properties allowed must survive
// the fold. This pins that invariant across the whole shipped catalog: if a
// future fold narrows a member (one-sided keyword inheritance, a shared
// property whose type disagrees, a required field dropped by intersection),
// the member's own example fails validation against the folded record here.
import { describe, expect, it } from 'vitest';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-execute-validate.js';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { ALL_UNFOLDED_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/unfolded.js';

const byAction = new Map<string, typeof ALL_UNFOLDED_CAPABILITY_RECORDS[number]>();
for (const record of ALL_UNFOLDED_CAPABILITY_RECORDS) {
  byAction.set(`${record.routing.parentTool}::${String(record.legacyIds[0]?.action)}`, record);
}

const pinFor = (record: (typeof ALL_CAPABILITY_RECORDS)[number], action: string): Record<string, unknown> => {
  const pair = record.legacyIds.find((legacy) => String(legacy.action) === action);
  return pair?.folded !== undefined && record.routing.dispatchBy !== undefined
    ? Object.fromEntries(Object.entries(pair.folded).map(([key, value]) => [key, value]))
    : {};
};

describe('every folded record subsumes its members', () => {
  it("every member's example input, with its folded pins applied, validates against the folded record's input schema", () => {
    const failures: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      for (const legacy of record.legacyIds) {
        const member = byAction.get(`${record.routing.parentTool}::${String(legacy.action)}`);
        // A folded pair may be hand-authored straight into the family record
        // (the volume data files); it has no unfolded member record to pin.
        if (member === undefined) continue;
        const example = member.examples[0]?.input;
        if (example === undefined) continue;
        const declaresAction = record.schemas.input.properties.action !== undefined;
        const { action: _memberAction, ...exampleRest } = example;
        const candidate = {
          ...exampleRest,
          ...pinFor(record, String(legacy.action)),
          ...(declaresAction ? { action: String(record.routing.dispatchAction) } : {}),
        };
        const violation = validateAgainstCapabilitySchema(candidate, record.schemas.input);
        if (violation !== undefined) {
          failures.push(`${member.id}: ${violation.message} (${violation.pointer})`);
        }
      }
    }
    expect(failures, `members rejected by their own fold:\n${failures.slice(0, 20).join('\n')}`).toEqual([]);
  });
});
