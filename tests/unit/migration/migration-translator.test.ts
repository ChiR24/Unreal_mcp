import { describe, expect, it } from 'vitest';

import { generateAliases } from '../../../src/tools/catalog/capabilities/migration/alias-generation.js';
import {
  migrationMap,
  resolveMigrationEntry
} from '../../../src/tools/catalog/capabilities/migration/migration-map.js';

const INVENTORY_OCCURRENCES = 1341;
const NATIVE_ROUTE_REMOVALS = 7;

// The migration map is DATA the live gateway reads: `checkMigration` in
// src/server/gateway/gateway-execute-lookup.ts resolves each legacy pair
// through it (plus the lossy rules) to refuse retired verbs and lossy shapes.
// The refusal BEHAVIOUR is covered end to end against that path in
// tests/unit/tools/unreal-gateway-execute-canonical.test.ts; this suite pins
// the data those refusals are derived from.
describe('Task 20 migration map — coverage and refusal data', () => {
  it('resolves every one of the 1,341 shipped occurrences to a canonical record or explicit typed removal', () => {
    // Given the audited normalization inventory (1,341 occurrences), 7 of which
    // are native raw routes explicitly retired (no-op/manual/dead).
    // When the migration map is built
    // Then every legacy occurrence resolves, and the retired routes are typed removals.
    expect(migrationMap.occurrenceCount).toBe(INVENTORY_OCCURRENCES);

    let resolved = 0;
    let removed = 0;
    for (const entry of migrationMap.entries.values()) {
      if (entry.disposition === 'removed') {
        removed += 1;
        expect(entry.removal).toBeDefined();
        expect(entry.removal?.guidance.length).toBeGreaterThan(0);
      } else if (entry.disposition === 'non-translatable') {
        expect(entry.nonTranslatable).toBeDefined();
      } else {
        resolved += 1;
        expect(entry.canonicalId).toBeDefined();
      }
    }
    expect(resolved + removed).toBe(migrationMap.entries.size);
    expect(removed).toBe(NATIVE_ROUTE_REMOVALS);
  });

  it('resolves an alias occurrence to the live record its own legacy pair selects', () => {
    const entry = resolveMigrationEntry('system_control', 'console_command');
    expect(entry?.disposition).toBe('alias');
    expect(entry?.canonicalId).toBe('system_control.console_command');
  });

  it('reaches every lossless entry by its own tool/action pair', () => {
    // The map is keyed by `tool::action`; the gateway looks entries up by the
    // pair it parsed off the request. A key built from anything else would make
    // an entry unreachable in production while still counting above.
    const lossless = [...migrationMap.entries.values()].filter(
      (e) => (e.disposition === 'canonical' || e.disposition === 'alias') && e.canonicalId
    );
    expect(lossless.length).toBeGreaterThan(1300);

    const unreachable: string[] = [];
    for (const entry of lossless) {
      const resolved = resolveMigrationEntry(entry.tool, entry.action);
      if (resolved?.legacyKey !== entry.legacyKey) {
        unreachable.push(entry.legacyKey);
      }
    }
    expect(unreachable).toEqual([]);
  });

  it('surfaces an unknown legacy call as an unresolved lookup (no silent fallback)', () => {
    expect(resolveMigrationEntry('manage_asset', 'this_verb_never_shipped')).toBeUndefined();
  });

  it('alias generation is deterministic and conflict-free', () => {
    const first = generateAliases();
    const second = generateAliases();
    expect(first.conflicts).toHaveLength(0);
    expect(first.aliases).toEqual(second.aliases);
    // The 5 inventory aliases are present.
    const consoleAlias = first.aliases.find(
      (a) => a.alias === 'system_control.console_command'
    );
    expect(consoleAlias?.canonicalId).toBe('system_control.console_command');
  });
});
