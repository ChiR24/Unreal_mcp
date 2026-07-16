import { describe, expect, it } from 'vitest';

import { secondCapabilitySource, validCapabilitySource } from './capability-record.test-support.js';
import {
  capabilityErrorPointers,
  createCapabilityRecord,
  parseCapabilityCatalog
} from './index.js';

function catalogRejectionPointers(input: unknown): readonly string[] {
  try {
    parseCapabilityCatalog(input);
    return [];
  } catch (error) {
    return capabilityErrorPointers(error);
  }
}

describe('Capability catalog collision validation', () => {
  it('rejects duplicate canonical IDs', () => {
    // Given
    const first = createCapabilityRecord(validCapabilitySource());
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({ ...secondSource, id: first.id });

    // When
    const pointers = catalogRejectionPointers([first, second]);

    // Then
    expect(pointers).toContain('/1/id');
  });

  it('rejects a later canonical ID equal to an earlier alias', () => {
    // Given a first record that aliases 'asset.remove', then a second record
    // whose canonical id IS that earlier alias (reverse collision)
    const first = createCapabilityRecord(validCapabilitySource());
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({ ...secondSource, id: first.aliases[0] });

    // When
    const pointers = catalogRejectionPointers([first, second]);

    // Then the later canonical id is rejected at the exact JSON pointer
    expect(pointers).toContain('/1/id');
  });

  it('rejects duplicate aliases within one record', () => {
    // Given
    const record = createCapabilityRecord(validCapabilitySource());

    // When
    const pointers = catalogRejectionPointers([
      { ...record, aliases: ['asset.remove', 'asset.remove'] }
    ]);

    // Then
    expect(pointers).toContain('/0/aliases/1');
  });

  it('rejects an alias colliding with another canonical ID', () => {
    // Given
    const first = createCapabilityRecord(validCapabilitySource());
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({ ...secondSource, aliases: [first.id] });

    // When
    const pointers = catalogRejectionPointers([first, second]);

    // Then
    expect(pointers).toContain('/1/aliases/0');
  });

  it('rejects aliases shared by separate records', () => {
    // Given
    const first = createCapabilityRecord(validCapabilitySource());
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({
      ...secondSource,
      aliases: [validCapabilitySource().aliases[0]]
    });

    // When
    const pointers = catalogRejectionPointers([first, second]);

    // Then
    expect(pointers).toContain('/1/aliases/0');
  });

  it('rejects duplicate legacy tool/action IDs', () => {
    // Given
    const first = createCapabilityRecord(validCapabilitySource());
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({
      ...secondSource,
      legacyIds: validCapabilitySource().legacyIds
    });

    // When
    const pointers = catalogRejectionPointers([first, second]);

    // Then
    expect(pointers).toContain('/1/legacyIds/0');
  });

  it('rejects duplicate legacy tool/action IDs within one record', () => {
    // Given a single valid record whose legacyIds repeat the same tool/action pair
    const record = createCapabilityRecord(validCapabilitySource());
    const repeatedLegacy = record.legacyIds[0];

    // When the catalog parses a single record carrying the duplicate legacy id
    const pointers = catalogRejectionPointers([
      { ...record, legacyIds: [repeatedLegacy, repeatedLegacy] }
    ]);

    // Then the later occurrence is reported at the exact JSON pointer
    expect(pointers).toContain('/0/legacyIds/1');
  });

  it('rejects a catalog record carrying a stale schema/content hash', () => {
    // Given a valid record, then a mutated clone whose content no longer matches
    const record = createCapabilityRecord(validCapabilitySource());
    const mutated = {
      ...record,
      discovery: { ...record.discovery, summary: 'Content changed after hashing.' }
    };

    // When
    const pointers = catalogRejectionPointers([mutated]);

    // Then the recomputed/verified hash mismatch is reported deterministically
    expect(pointers).toContain('/0/hashes/content');
  });

  it('accepts distinct canonical, alias, and legacy IDs', () => {
    // Given
    const first = createCapabilityRecord(validCapabilitySource());
    const second = createCapabilityRecord(secondCapabilitySource());

    // When
    const catalog = parseCapabilityCatalog([first, second]);

    // Then
    expect(catalog).toEqual([first, second]);
  });
});
