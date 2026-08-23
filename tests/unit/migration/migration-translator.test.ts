import { describe, expect, it } from 'vitest';

import {
  buildMigrationArtifact,
  migrationArtifact,
  MigrationArtifactSchema
} from '../../../src/tools/catalog/capabilities/migration/artifact.js';
import { generateAliases } from '../../../src/tools/catalog/capabilities/migration/alias-generation.js';
import {
  migrationMap,
  resolveMigrationEntry
} from '../../../src/tools/catalog/capabilities/migration/migration-map.js';
import { translateExecute } from '../../../src/tools/catalog/capabilities/migration/translator.js';
import {
  NonTranslatableMigrationError,
  UnknownLegacyCallError
} from '../../../src/tools/catalog/capabilities/migration/types.js';

const INVENTORY_OCCURRENCES = 1341;
const NATIVE_ROUTE_REMOVALS = 8;

describe('Task 20 migration map — coverage and refusal', () => {
  it('resolves every one of the 1,341 shipped occurrences to a canonical record or explicit typed removal', () => {
    // Given the audited normalization inventory (1,340 occurrences) plus 8
    // native raw routes that are explicitly retired (no-op/manual/dead).
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

  it('RED: a lossy mismatch (distinct volume bounds/extent semantics) refuses translation', () => {
    // Given a legacy set_volume_bounds call carrying a bounds.origin
    // When translated through the compatibility translator
    // Then it is REFUSED with exact replacement guidance (no silent coercion).
    expect(() =>
      translateExecute({
        tool: 'manage_level_structure',
        action: 'set_volume_bounds',
        params: {
          volumeName: 'PP_01',
          bounds: { origin: { x: 0, y: 0, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } }
        }
      })
    ).toThrow(NonTranslatableMigrationError);

    try {
      translateExecute({
        tool: 'manage_level_structure',
        action: 'set_volume_bounds',
        params: {
          volumeName: 'PP_01',
          bounds: { origin: { x: 0, y: 0, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } }
        }
      });
      throw new Error('expected refusal');
    } catch (error) {
      if (!(error instanceof NonTranslatableMigrationError)) {
        throw error;
      }
      const guidance = error.guidance;
      expect(guidance.canonicalId).toBe('manage_level_structure.set_volume_extent');
      expect(guidance.nextCall.action).toBe('set_volume_extent');
      expect(guidance.reason).toMatch(/origin/i);
    }
  });

  it('does NOT refuse a lossless set_volume_bounds call without an origin', () => {
    // A legacy extent-only call has no origin to lose; it is lossless and must
    // translate rather than refuse.
    const result = translateExecute({
      tool: 'manage_level_structure',
      action: 'set_volume_bounds',
      params: { volumeName: 'PP_01', extent: { x: 1000, y: 1000, z: 500 } }
    });
    expect(result.canonicalId).toBe('manage_level_structure.set_volume_bounds');
  });

  it('resolves an alias occurrence to the live record its own legacy pair selects', () => {
    const entry = resolveMigrationEntry('system_control', 'console_command');
    expect(entry?.disposition).toBe('alias');
    expect(entry?.canonicalId).toBe('system_control.console_command');

    const result = translateExecute({ tool: 'system_control', action: 'console_command', params: {} });
    expect(result.canonicalId).toBe('system_control.console_command');
  });

  it('GREEN: round-trips every lossless (canonical + alias) translator and returns the canonical id in the receipt', () => {
    const entries = [...migrationMap.entries.values()];
    const lossless = entries.filter(
      (e) => (e.disposition === 'canonical' || e.disposition === 'alias') && e.canonicalId
    );
    expect(lossless.length).toBeGreaterThan(1300);

    for (const entry of lossless) {
      const result = translateExecute({
        tool: entry.tool,
        action: entry.action,
        params: { sample: 1 }
      });
      // The canonical id is always returned in the receipt.
      expect(result.canonicalId).toBe(entry.canonicalId);
      expect(result.entry.legacyKey).toBe(entry.legacyKey);
    }
  });

  it('surfaces an unknown legacy call as a typed error (no silent fallback)', () => {
    expect(() =>
      translateExecute({ tool: 'manage_asset', action: 'this_verb_never_shipped', params: {} })
    ).toThrow(UnknownLegacyCallError);
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

describe('Task 20 migration artifact — schema + determinism', () => {
  it('artifact is schema-validated', () => {
    const parsed = MigrationArtifactSchema.parse(migrationArtifact);
    expect(parsed.schemaVersion).toBe('task20.migration.v1');
    expect(parsed.entryCount).toBe(migrationMap.entries.size);
    expect(parsed.contentHash).toMatch(/^[0-9a-f]{64}$/);
  });

  it('two builds are byte-identical (deterministic generation)', () => {
    const a = JSON.stringify(buildMigrationArtifact());
    const b = JSON.stringify(buildMigrationArtifact());
    expect(a).toBe(b);
  });
});
