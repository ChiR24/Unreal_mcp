import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { executeTargetIndex, resolveExecuteTarget } from '../../../src/server/gateway/gateway-execute-resolve.js';
import { migrationMap } from '../../../src/tools/catalog/capabilities/migration/migration-map.js';

const EXPECTED_RECORDS = 1380;
const EXPECTED_PARENTS = 23;
const REMOVED_ERROR_CODE = 'CAPABILITY_REMOVED';

const UNKNOWN_ACTION_CODE = 'UNKNOWN_ACTION';

/**
 * A "typed explicit removal" ships in exactly two shapes, and both must refuse:
 *   CAPABILITY_REMOVED - the record still exists for discovery, but execute
 *                        refuses with removal guidance (Task 21 no-op stubs).
 *   UNKNOWN_ACTION     - no record exists at all, so the legacy pair was never
 *                        publicly reachable and is refused before migration runs.
 * Neither shape may ever silently succeed.
 */
const EXPECTED_REMOVED_PAIRS = [
  'animation_physics::assign_cloth_asset_to_mesh',
  'animation_physics::create_pose_library',
  'animation_physics::set_retarget_chain_mapping',
];

const EXPECTED_UNREACHABLE_PAIRS = [
  'animation_physics::preview_physics',
  'manage_blueprint::apply_style_to_widget',
  'manage_blueprint::get_nodes',
  'manage_blueprint::set_animation_speed',
  'manage_effect::shadowed_effect_module_stubs',
];

describe('Task 29 - canonical and deprecated legacy client paths normalize identically', () => {
  it('all 1,380 legacy {tool, action} pairs reach a capability or a typed removal', () => {
    const index = executeTargetIndex();
    const unexplained: string[] = [];
    const removals: string[] = [];
    let resolved = 0;

    for (const record of ALL_CAPABILITY_RECORDS) {
      const legacy = record.legacyIds[0];
      if (legacy === undefined) {
        unexplained.push(`${String(record.id)} has no legacy pair`);
        continue;
      }
      const resolution = resolveExecuteTarget(
        { tool: String(legacy.tool), action: String(legacy.action) },
        index,
      );

      if (resolution.ok) {
        if (String(resolution.target.record.id) !== String(record.id)) {
          unexplained.push(
            `${String(legacy.tool)}::${String(legacy.action)} resolved to ${String(resolution.target.record.id)}, expected ${String(record.id)}`,
          );
          continue;
        }
        resolved += 1;
        continue;
      }

      if (resolution.failure.errorCode === REMOVED_ERROR_CODE) {
        expect(resolution.failure.message.length, `${String(record.id)} removal lacks guidance`).toBeGreaterThan(0);
        removals.push(`${String(legacy.tool)}::${String(legacy.action)}`);
        continue;
      }

      unexplained.push(
        `${String(legacy.tool)}::${String(legacy.action)} -> ${resolution.failure.errorCode}: ${resolution.failure.message}`,
      );
    }

    expect(unexplained, `unexplained legacy outcomes:\n${unexplained.slice(0, 10).join('\n')}`).toEqual([]);
    expect(removals.sort()).toEqual(EXPECTED_REMOVED_PAIRS);
    expect(resolved + removals.length).toBe(EXPECTED_RECORDS);
  });

  it('the canonical form and the legacy form reach the identical outcome for all 1,380', () => {
    const index = executeTargetIndex();
    const divergences: string[] = [];
    let compared = 0;

    for (const record of ALL_CAPABILITY_RECORDS) {
      const legacy = record.legacyIds[0];
      if (legacy === undefined) continue;

      const canonical = resolveExecuteTarget({ capability: String(record.id) }, index);
      const viaLegacy = resolveExecuteTarget(
        { tool: String(legacy.tool), action: String(legacy.action) },
        index,
      );

      if (canonical.ok !== viaLegacy.ok) {
        divergences.push(
          `${String(record.id)}: canonical.ok=${canonical.ok} but legacy.ok=${viaLegacy.ok}`,
        );
        continue;
      }

      if (!canonical.ok && !viaLegacy.ok) {
        if (canonical.failure.errorCode !== viaLegacy.failure.errorCode) {
          divergences.push(
            `${String(record.id)}: canonical errorCode ${canonical.failure.errorCode} != legacy ${viaLegacy.failure.errorCode}`,
          );
          continue;
        }
        if (canonical.failure.message !== viaLegacy.failure.message) {
          divergences.push(`${String(record.id)}: removal guidance differs between forms`);
          continue;
        }
        compared += 1;
        continue;
      }

      if (canonical.ok && viaLegacy.ok) {
        if (String(canonical.target.record.id) !== String(viaLegacy.target.record.id)) {
          divergences.push(
            `${String(record.id)}: canonical -> ${String(canonical.target.record.id)}, legacy -> ${String(viaLegacy.target.record.id)}`,
          );
          continue;
        }
        if (JSON.stringify(canonical.target.legacy) !== JSON.stringify(viaLegacy.target.legacy)) {
          divergences.push(
            `${String(record.id)}: dispatch legacy pair differs ${JSON.stringify(canonical.target.legacy)} vs ${JSON.stringify(viaLegacy.target.legacy)}`,
          );
          continue;
        }
        compared += 1;
      }
    }

    expect(divergences, `canonical/legacy divergence:\n${divergences.slice(0, 10).join('\n')}`).toEqual([]);
    expect(compared).toBe(EXPECTED_RECORDS);
  });

  it('supplying a conflicting canonical id and legacy pair is a typed refusal, never a silent pick', () => {
    const index = executeTargetIndex();
    const first = ALL_CAPABILITY_RECORDS[0];
    const other = ALL_CAPABILITY_RECORDS[1];
    if (first === undefined || other === undefined) throw new Error('need two records');
    const otherLegacy = other.legacyIds[0];
    if (otherLegacy === undefined) throw new Error('need a legacy pair');

    const resolution = resolveExecuteTarget(
      {
        capability: String(first.id),
        tool: String(otherLegacy.tool),
        action: String(otherLegacy.action),
      },
      index,
    );

    expect(resolution.ok).toBe(false);
    if (resolution.ok) throw new Error('unreachable');
    expect(resolution.failure.errorCode).toBe('FORM_CONFLICT');
  });

  it('an unknown legacy pair is refused with a typed error and guidance, never resolved', () => {
    const index = executeTargetIndex();
    const resolution = resolveExecuteTarget(
      { tool: 'manage_asset', action: '__task29_no_such_action__' },
      index,
    );
    expect(resolution.ok).toBe(false);
    if (resolution.ok) throw new Error('unreachable');
    expect(resolution.failure.errorCode.length).toBeGreaterThan(0);
    expect(resolution.failure.message.length).toBeGreaterThan(0);
  });

  it('the 23 private parent routes each expose a non-empty action set', () => {
    const index = executeTargetIndex();
    expect(index.parentTools.length).toBe(EXPECTED_PARENTS);

    let totalActions = 0;
    for (const parent of index.parentTools) {
      const actions = index.actionsByParentTool.get(parent) ?? [];
      expect(actions.length, `parent ${parent} has no actions`).toBeGreaterThan(0);
      totalActions += actions.length;
    }
    expect(totalActions).toBe(EXPECTED_RECORDS);
  });
});

describe('Task 29 - every migration disposition is checked against the LIVE registry', () => {
  it('every non-removed migration key is a legacy pair the live registry actually serves', () => {
    const servedPairs = new Set<string>();
    for (const record of ALL_CAPABILITY_RECORDS) {
      for (const legacy of record.legacyIds) {
        servedPairs.add(`${String(legacy.tool)}::${String(legacy.action)}`);
      }
    }

    const unserved: string[] = [];
    let checked = 0;
    for (const [key, entry] of migrationMap.entries) {
      if (entry.disposition === 'removed') continue;
      checked += 1;
      if (!servedPairs.has(key)) unserved.push(`${key} (${entry.disposition})`);
    }

    expect(unserved, `migration keys with no live capability:\n${unserved.slice(0, 10).join('\n')}`).toEqual([]);
    expect(checked).toBeGreaterThan(0);
    expect(servedPairs.size).toBe(EXPECTED_RECORDS);
  });

  it('every removed migration key is refused at execute with a typed error', () => {
    const index = executeTargetIndex();
    const removed = [...migrationMap.entries.entries()].filter(([, e]) => e.disposition === 'removed');

    const wrong: string[] = [];
    const removedWithRecord: string[] = [];
    const unreachable: string[] = [];

    for (const [key] of removed) {
      const [tool, action] = key.split('::');
      if (tool === undefined || action === undefined) {
        wrong.push(`${key} is not a tool::action key`);
        continue;
      }
      const resolution = resolveExecuteTarget({ tool, action }, index);

      if (resolution.ok) {
        wrong.push(`${key} is marked removed but execute resolved it to ${String(resolution.target.record.id)}`);
        continue;
      }

      expect(resolution.failure.message.length, `${key} refusal lacks guidance`).toBeGreaterThan(0);

      if (resolution.failure.errorCode === REMOVED_ERROR_CODE) {
        removedWithRecord.push(key);
      } else if (resolution.failure.errorCode === UNKNOWN_ACTION_CODE) {
        unreachable.push(key);
      } else {
        wrong.push(`${key} refused with unexpected code ${resolution.failure.errorCode}`);
      }
    }

    expect(wrong, `removal contract violations:\n${wrong.slice(0, 10).join('\n')}`).toEqual([]);
    expect(removedWithRecord.sort()).toEqual(EXPECTED_REMOVED_PAIRS);
    expect(unreachable.sort()).toEqual(EXPECTED_UNREACHABLE_PAIRS);
    expect(removedWithRecord.length + unreachable.length).toBe(removed.length);
  });
});
