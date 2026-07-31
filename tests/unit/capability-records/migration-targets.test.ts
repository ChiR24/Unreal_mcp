import { describe, expect, it } from 'vitest';

import {
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../../src/tools/catalog/capabilities/identifiers.js';
import type { LegacyCapabilityId } from '../../../src/tools/catalog/capabilities/model.js';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { generateAliases } from '../../../src/tools/catalog/capabilities/migration/alias-generation.js';
import { LOSSY_RULES } from '../../../src/tools/catalog/capabilities/migration/lossy-translations.js';
import { migrationMap } from '../../../src/tools/catalog/capabilities/migration/migration-map.js';
import type { LegacyKey } from '../../../src/tools/catalog/capabilities/migration/types.js';
import {
  executeTargetIndex,
  resolveExecuteTarget,
} from '../../../src/server/gateway/gateway-execute-resolve.js';

/**
 * Task 29 - migration entries must target LIVE capability records.
 *
 * The Task 20 map was built straight off the normalization inventory, whose
 * `canonicalId` is the Task 5 analysis notation (`cap:<namespace>:<action>`).
 * That notation never shipped: no capability record carries such an id, so
 * every non-removed entry pointed at nothing. The shipped identity is the
 * dotted record id, and the only total, collision-free way to reach it is the
 * entry's OWN legacy `{tool, action}` pair - five stale ids (`cap:shared:*`)
 * fan out to two distinct live records each, so the stale id cannot be keyed.
 */

const EXPECTED_ENTRIES = 1340;
const EXPECTED_NON_REMOVED = 1332;
const EXPECTED_REMOVALS = 8;
const EXPECTED_ALIASES = 5;
const STALE_NAMESPACE = /^cap:/;

const pairKey = (legacy: LegacyCapabilityId): LegacyKey => `${legacy.tool}::${legacy.action}`;

const namedKey = (tool: string, action: string): LegacyKey =>
  `${LegacyToolNameSchema.parse(tool)}::${LegacyActionNameSchema.parse(action)}`;

/** `tool::action` -> the single live record that declares that legacy pair. */
const liveIdByLegacyPair = new Map<LegacyKey, string>(
  ALL_CAPABILITY_RECORDS.flatMap((record) =>
    record.legacyIds.map((legacy) => [pairKey(legacy), String(record.id)] as const),
  ),
);

const liveIds = new Set(ALL_CAPABILITY_RECORDS.map((record) => String(record.id)));

const entries = [...migrationMap.entries.values()];
const nonRemoved = entries.filter((entry) => entry.disposition !== 'removed');
const removed = entries.filter((entry) => entry.disposition === 'removed');

describe('Task 29 - migration canonical targets resolve to live capability records', () => {
  it('partitions the map into 1,332 non-removed entries and 8 explicit removals', () => {
    // Given the migration map built from the audited inventory plus retired native routes
    // When the entries are partitioned by disposition
    // Then the shipped shape is exactly 1,340 = 1,332 live + 8 removed.
    expect(entries).toHaveLength(EXPECTED_ENTRIES);
    expect(nonRemoved).toHaveLength(EXPECTED_NON_REMOVED);
    expect(removed).toHaveLength(EXPECTED_REMOVALS);
  });

  it('resolves every non-removed canonicalId to a live capability record', () => {
    // Given every non-removed entry
    // When its canonicalId is looked up in the canonical record source
    // Then it names a record that actually ships.
    const dangling = nonRemoved
      .filter((entry) => entry.canonicalId === undefined || !liveIds.has(String(entry.canonicalId)))
      .map((entry) => `${entry.legacyKey} -> ${String(entry.canonicalId)}`);

    expect(dangling, `dangling canonical targets:\n${dangling.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('points each entry at the record selected by that entry own legacy pair', () => {
    // Given the five `cap:shared:*` ids that fan out to two live records each
    // When each entry is resolved by its own `{tool, action}` rather than by the stale id
    // Then control_editor and system_control reach their own distinct records.
    const misrouted = nonRemoved
      .filter((entry) => liveIdByLegacyPair.get(entry.legacyKey) !== String(entry.canonicalId))
      .map(
        (entry) =>
          `${entry.legacyKey} -> ${String(entry.canonicalId)} `
          + `(expected ${String(liveIdByLegacyPair.get(entry.legacyKey))})`,
      );

    expect(misrouted, `misrouted entries:\n${misrouted.slice(0, 10).join('\n')}`).toEqual([]);

    // The fan-out pairs specifically must NOT collapse onto one shared target.
    const editorConsole = migrationMap.entries.get(namedKey('control_editor', 'console_command'));
    const systemConsole = migrationMap.entries.get(namedKey('system_control', 'console_command'));
    expect(String(editorConsole?.canonicalId)).toBe('control_editor.console_command');
    expect(String(systemConsole?.canonicalId)).toBe('system_control.console_command');
  });

  it('maps the 1,332 non-removed entries injectively with zero collisions', () => {
    // Given the non-removed entries
    // When their canonical targets are collected
    // Then no two legacy pairs share a capability record.
    const owners = new Map<string, LegacyKey>();
    const collisions: string[] = [];
    for (const entry of nonRemoved) {
      const target = String(entry.canonicalId);
      const owner = owners.get(target);
      if (owner === undefined) owners.set(target, entry.legacyKey);
      else collisions.push(`${target} claimed by ${owner} and ${entry.legacyKey}`);
    }

    expect(collisions, `colliding targets:\n${collisions.slice(0, 10).join('\n')}`).toEqual([]);
    expect(owners.size).toBe(EXPECTED_NON_REMOVED);
  });

  it('leaves no `cap:` namespace reference in any migration, alias or lossy target', () => {
    // Given the entries, the generated aliases and the curated lossy rules
    // When every canonical reference they publish is inspected
    // Then the Task 5 analysis namespace appears nowhere.
    const { aliases } = generateAliases();
    const published: string[] = [
      ...entries.flatMap((entry) => [
        ...(entry.canonicalId === undefined ? [] : [String(entry.canonicalId)]),
        ...(entry.removal?.replacement === undefined ? [] : [String(entry.removal.replacement)]),
        ...(entry.nonTranslatable?.guidance.canonicalId === undefined
          ? []
          : [String(entry.nonTranslatable.guidance.canonicalId)]),
      ]),
      ...aliases.map((alias) => String(alias.canonicalId)),
      ...LOSSY_RULES.map((rule) => String(rule.canonicalId)),
    ];
    const stale = published.filter((reference) => STALE_NAMESPACE.test(reference));

    expect(stale, `stale cap: references:\n${[...new Set(stale)].slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('keeps every removal explicit and canonicalId-free', () => {
    // Given the 8 retired native routes
    // When their entries are inspected
    // Then each carries removal guidance and names no canonical target at all.
    for (const entry of removed) {
      expect(entry.canonicalId, `${entry.legacyKey} must not name a canonical target`).toBeUndefined();
      expect(entry.removal?.replacement, `${entry.legacyKey} must not name a replacement`).toBeUndefined();
      expect(entry.removal?.since.length ?? 0, `${entry.legacyKey} lacks a removal version`).toBeGreaterThan(0);
      expect(entry.removal?.guidance.length ?? 0, `${entry.legacyKey} lacks removal guidance`).toBeGreaterThan(0);
      expect(entry.deprecation.status).toBe('removed');
    }
  });

  it('generates 5 alias targets on live records with zero conflicts', () => {
    // Given alias generation over the repaired map
    // When the generated aliases are resolved against the record source
    // Then all 5 shipped aliases target live records and nothing conflicts.
    const { aliases, conflicts } = generateAliases();

    expect(conflicts).toEqual([]);
    expect(aliases).toHaveLength(EXPECTED_ALIASES);
    const dangling = aliases
      .filter((alias) => !liveIds.has(String(alias.canonicalId)))
      .map((alias) => `${String(alias.alias)} -> ${String(alias.canonicalId)}`);
    expect(dangling, `dangling alias targets:\n${dangling.join('\n')}`).toEqual([]);
  });

  it('points the curated lossy rule at the live extent-only record', () => {
    // Given the single curated lossy rule (volume bounds vs extent)
    // When its replacement target is resolved
    // Then it names the shipped extent capability, not the analysis notation.
    const boundsKey = namedKey('manage_level_structure', 'set_volume_bounds');
    const rule = LOSSY_RULES.find((candidate) => candidate.legacyKey === boundsKey);

    expect(String(rule?.canonicalId)).toBe('manage_level_structure.set_volume_extent');
    expect(liveIds.has(String(rule?.canonicalId))).toBe(true);
  });

  it('keeps live execute resolution agreeing with every repaired canonical target', () => {
    // Given the production execute resolver
    // When each non-removed legacy pair is resolved through it
    // Then the record it selects is exactly the entry canonicalId (behavior unchanged).
    const index = executeTargetIndex();
    const divergent: string[] = [];

    for (const entry of nonRemoved) {
      const resolution = resolveExecuteTarget({ tool: entry.tool, action: entry.action }, index);
      if (!resolution.ok) {
        divergent.push(`${entry.legacyKey} refused: ${resolution.failure.errorCode}`);
        continue;
      }
      if (String(resolution.target.record.id) !== String(entry.canonicalId)) {
        divergent.push(
          `${entry.legacyKey} executes ${String(resolution.target.record.id)} `
          + `but maps to ${String(entry.canonicalId)}`,
        );
      }
    }

    expect(divergent, `execute divergence:\n${divergent.slice(0, 10).join('\n')}`).toEqual([]);
  });

  it('still refuses every removed legacy pair at execute time', () => {
    // Given the 8 retired routes
    // When they are resolved through the production execute resolver
    // Then each is refused as removed or as an action that never shipped.
    const index = executeTargetIndex();
    const accepted = removed
      .filter((entry) => resolveExecuteTarget({ tool: entry.tool, action: entry.action }, index).ok)
      .map((entry) => entry.legacyKey);

    expect(accepted, `removed pairs that still execute:\n${accepted.join('\n')}`).toEqual([]);
  });
});
