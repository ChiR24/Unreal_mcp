/**
 * tests/unit/capability-records/record-pointer-honesty.test.ts
 *
 * Record text that sends a caller elsewhere ("use add_ability_task") must name something callable: an action, a
 * selector value, a parameter, a parent tool or a capability. Nine such pointers named actions that were removed or
 * never built (grant_ability, configure_hitscan, set_weapon_stats ...), so a model that followed one was refused
 * UNKNOWN_ACTION.
 */

import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';

const collectEnums = (node: unknown, into: Set<string>): void => {
  if (Array.isArray(node)) {
    for (const item of node) collectEnums(item, into);
    return;
  }
  if (node === null || typeof node !== 'object') return;
  for (const [key, value] of Object.entries(node)) {
    if (key === 'enum' && Array.isArray(value)) {
      for (const entry of value) if (typeof entry === 'string') into.add(entry);
    }
    collectEnums(value, into);
  }
};

const callableNames = (): ReadonlySet<string> => {
  const known = new Set<string>();
  for (const record of ALL_CAPABILITY_RECORDS) {
    for (const part of String(record.id).split('.')) known.add(part);
    known.add(record.routing.parentTool);
    for (const pair of record.legacyIds) {
      known.add(pair.tool);
      known.add(pair.action);
    }
    for (const alias of record.aliases) for (const part of String(alias).split('.')) known.add(part);
    collectEnums(record.schemas.input, known);
    for (const name of Object.keys(record.schemas.input.properties ?? {})) known.add(name);
  }
  return known;
};

describe('record text points only at callable names', () => {
  it('names an action, selector value, parameter, tool or capability after every "use"', () => {
    const known = callableNames();
    const offenders: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const { summary, whenToUse, whenNotToUse } = record.discovery;
      for (const text of [summary, ...whenToUse, ...whenNotToUse]) {
        for (const match of text.matchAll(/\b[Uu]se ([a-z]+_[a-z_]+)/gu)) {
          const name = match[1] ?? '';
          if (!known.has(name)) offenders.push(`${String(record.id)} names ${name}: "${text}"`);
        }
      }
    }
    expect(offenders).toEqual([]);
  });
});
