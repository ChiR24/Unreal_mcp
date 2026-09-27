// Plan Todo 12 (BB-053) — an ambiguous selector is refused with every owner
// named, never resolved by silent last-writer-wins.
//
// `byAlias` and `byLegacyPair` are single-winner maps. Before this, a second
// record claiming the same alias or tool+action overwrote the first, so
// discovery answered for whichever record sorted last and execute could dispatch
// a capability the caller never asked for.

import { describe, expect, it } from 'vitest';

import {
  capabilityIndex,
  detectIndexConflicts,
  legacyPairKey
} from '../../../src/server/gateway/gateway-capability-index.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema
} from '../../../src/tools/catalog/capabilities/identifiers.js';

function baseRecord(): CapabilityRecord {
  const [first] = capabilityIndex().records;
  if (!first) throw new Error('the generated registry is empty');
  return first;
}

function withId(base: CapabilityRecord, id: string): CapabilityRecord {
  return { ...base, id: CapabilityIdSchema.parse(id) };
}

describe('todo12: detectIndexConflicts names every owner of a contested selector', () => {
  it('reports both owners of a shared alias', () => {
    const base = baseRecord();
    const shared = CapabilityAliasSchema.parse('fixture.shared');
    const left: CapabilityRecord = { ...withId(base, 'fixture.left'), aliases: [shared] };
    const right: CapabilityRecord = { ...withId(base, 'fixture.right'), aliases: [shared] };

    const conflicts = detectIndexConflicts([left, right]);
    const owners = conflicts.aliasConflicts.get('fixture.shared');

    expect(owners).toEqual(['fixture.left', 'fixture.right']);
  });

  it('reports both owners of a shared legacy pair', () => {
    const base = baseRecord();
    const pair = base.legacyIds[0];
    expect(pair).toBeDefined();
    if (!pair) return;

    const left = withId(base, 'fixture.left');
    const right = withId(base, 'fixture.right');

    const conflicts = detectIndexConflicts([left, right]);
    const owners = conflicts.legacyPairConflicts.get(legacyPairKey(pair.tool, pair.action));

    expect(owners).toEqual(['fixture.left', 'fixture.right']);
  });

  it('does not treat an alias that merely equals another capability id as a conflict', () => {
    const base = baseRecord();
    const owner: CapabilityRecord = { ...withId(base, 'fixture.owner'), aliases: [] };
    const shadow: CapabilityRecord = {
      ...withId(base, 'fixture.shadow'),
      aliases: [CapabilityAliasSchema.parse('fixture.owner')]
    };

    const conflicts = detectIndexConflicts([owner, shadow]);

    expect(conflicts.aliasConflicts.size).toBe(0);
  });

  it('does not report a single record that declares the same alias once', () => {
    const base = baseRecord();
    const solo: CapabilityRecord = {
      ...withId(base, 'fixture.solo'),
      aliases: [CapabilityAliasSchema.parse('fixture.only')]
    };

    const conflicts = detectIndexConflicts([solo]);

    expect(conflicts.aliasConflicts.size).toBe(0);
    expect(conflicts.legacyPairConflicts.size).toBe(0);
  });

  it('finds zero conflicts in the real generated catalogue', () => {
    const conflicts = detectIndexConflicts(capabilityIndex().records);

    expect([...conflicts.aliasConflicts.keys()]).toEqual([]);
    expect([...conflicts.legacyPairConflicts.keys()]).toEqual([]);
  });
});
