/**
 * Contract for authoring a capability AFTER the legacy migration.
 *
 * The normalization inventory audits the pre-gateway 23-tool surface: every
 * `{tool, action}` pair that actually shipped. `extractOccurrences()` derives
 * that audit from each record's `legacyIds`, and `REVIEWED_METRICS` pins the
 * result at 1,335. Those two numbers coincide with the record count only
 * because every migrated record carries exactly one legacy pair — an artifact
 * of the 1:1 migration, not an invariant.
 *
 * A capability authored after the migration has a live `{tool, action}` pair
 * but no historical one, so `legacyIds` would otherwise have to lie in one of
 * two directions: declare the pair and inflate an audit of what shipped, or
 * omit it and lose the action enum that `deriveParents()` builds from exactly
 * that field. `normalization.provenance` resolves the ambiguity as data.
 *
 * These tests pin all four edges of that contract, including the two
 * fail-closed directions: an unmarked addition still trips the gate, and a
 * marked-in-error existing record trips it too.
 */

import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../../src/tools/catalog/capabilities/records/aggregate.js';
import { createCapabilityRecord } from '../../src/tools/catalog/capabilities/parser.js';
import { buildInventory } from '../../src/tools/catalog/capabilities/normalization/build.js';
import { REVIEWED_METRICS } from '../../src/tools/catalog/capabilities/normalization/adjudicate.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { deriveParents } from '../../scripts/canonical-registry/parent-derivation.js';
import {
  buildExecuteTargetIndex,
  resolveExecuteTarget,
} from '../../src/server/gateway/gateway-execute-resolve.js';
import { getActionValues } from '../../src/server/gateway/gateway-shared.js';

const PARENT = 'manage_blueprint';
const NEW_ACTION = 'audit_exec_fan_in';

const BASE = ALL_CAPABILITY_RECORDS.find((record) => record.routing.parentTool === PARENT);
if (BASE === undefined) {
  throw new Error(`fixture base record for ${PARENT} is missing from the aggregate`);
}

/**
 * A mutable authoring source cloned off a real record.
 *
 * Cloned rather than spread so no test can reach the frozen exported aggregate,
 * and `hashes` is dropped because `createCapabilityRecord` mints them.
 */
function authoringSource(overrides: Readonly<Record<string, unknown>>): Record<string, unknown> {
  const source = JSON.parse(JSON.stringify(BASE)) as Record<string, unknown>;
  delete source.hashes;
  return { ...source, ...overrides };
}

function authoredCapability(provenance?: 'post-migration'): CapabilityRecord {
  const normalization = {
    ...(BASE as CapabilityRecord).normalization,
    ...(provenance === undefined ? {} : { provenance }),
  };
  return createCapabilityRecord(
    authoringSource({
      id: 'blueprint.audit_exec_fan_in',
      aliases: [],
      legacyIds: [{ tool: PARENT, action: NEW_ACTION }],
      routing: { ...(BASE as CapabilityRecord).routing, dispatchAction: NEW_ACTION },
      normalization,
    }),
  );
}

function markingOneMigratedRecord(): readonly CapabilityRecord[] {
  let marked = false;
  return ALL_CAPABILITY_RECORDS.map((record) => {
    if (marked || record.routing.parentTool !== PARENT) return record;
    marked = true;
    return {
      ...record,
      normalization: { ...record.normalization, provenance: 'post-migration' as const },
    };
  });
}

function blueprintActions(records: readonly CapabilityRecord[]): readonly string[] {
  const parent = deriveParents(records).find((tool) => tool.name === PARENT);
  if (parent === undefined) throw new Error(`${PARENT} missing from derived parents`);
  return getActionValues(parent);
}

describe('S1 a post-migration capability is skipped by the audit yet stays routable', () => {
  it('S1: the audited occurrence total does not move', () => {
    const inventory = buildInventory([...ALL_CAPABILITY_RECORDS, authoredCapability('post-migration')]);

    expect(inventory.metrics.occurrenceCount).toBe(REVIEWED_METRICS.occurrenceCount);
    expect(inventory.occurrences).toHaveLength(REVIEWED_METRICS.occurrenceCount);
    expect(inventory.occurrences.some((entry) => entry.action === NEW_ACTION)).toBe(false);
  });

  it('S1: the action still enters the parent action enum', () => {
    const before = blueprintActions(ALL_CAPABILITY_RECORDS);
    const after = blueprintActions([...ALL_CAPABILITY_RECORDS, authoredCapability('post-migration')]);

    expect(before).not.toContain(NEW_ACTION);
    expect(after).toContain(NEW_ACTION);
    expect(after).toHaveLength(before.length + 1);
  });

  it('S1: execute resolves it by tool and action, not only by capability id', () => {
    const index = buildExecuteTargetIndex([
      ...ALL_CAPABILITY_RECORDS,
      authoredCapability('post-migration'),
    ]);

    const resolution = resolveExecuteTarget({ tool: PARENT, action: NEW_ACTION, params: {} }, index);

    expect(resolution.ok).toBe(true);
  });
});

describe('S2/S3 the audit total is fail-closed in both directions', () => {
  it('S2: an unmarked addition still trips the reviewed-metric blocker', () => {
    expect(() => buildInventory([...ALL_CAPABILITY_RECORDS, authoredCapability()])).toThrow(
      /occurrenceCount: source produced 1336/,
    );
  });

  it('S3: marking an already-migrated capability trips it too', () => {
    expect(() => buildInventory(markingOneMigratedRecord())).toThrow(
      /occurrenceCount: source produced 1334/,
    );
  });
});

describe('S5 legacyIds is the live routing identity, not optional history', () => {
  it('S5: a capability declaring no legacy pair is rejected at authoring time', () => {
    const source = authoringSource({
      id: 'blueprint.audit_exec_fan_in',
      aliases: [],
      legacyIds: [],
    });

    expect(() => createCapabilityRecord(source)).toThrow(/legacyIds/);
  });

  it('S5: the same capability is accepted once it declares its pair', () => {
    const source = authoringSource({
      id: 'blueprint.audit_exec_fan_in',
      aliases: [],
      legacyIds: [{ tool: PARENT, action: 'audit_exec_fan_in' }],
    });

    const minted: CapabilityRecord = createCapabilityRecord(source);
    expect(minted.legacyIds).toHaveLength(1);
    expect(minted.legacyIds[0].action).toBe('audit_exec_fan_in');
  });
});
