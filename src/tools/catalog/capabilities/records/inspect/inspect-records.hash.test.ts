/**
 * Hash-parity tests for the inspect capability record catalog.
 *
 * Proves: every record hash matches a fresh recompute from its source, a JSON
 * round-trip preserves all 36 records with identical hashes, and
 * createCapabilityRecord is deterministic. Does not touch the shared core
 * builder, aggregate, pilots, or native code.
 */
import { describe, expect, it } from 'vitest';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import { INSPECT_RECORDS, INSPECT_SOURCES } from './index.js';

describe('inspect hash parity: TS source, JSON round-trip, and recompute', () => {
	it('every record hash matches a fresh recompute from its source', () => {
		for (let i = 0; i < INSPECT_SOURCES.length; i++) {
			const recomputed = createCapabilityRecord(INSPECT_SOURCES[i]);
			expect(recomputed.hashes.schema).toBe(INSPECT_RECORDS[i].hashes.schema);
			expect(recomputed.hashes.content).toBe(INSPECT_RECORDS[i].hashes.content);
		}
	});

	it('JSON round-trip preserves all 36 records with identical hashes', () => {
		const json = JSON.stringify(INSPECT_RECORDS);
		const restored = JSON.parse(json) as typeof INSPECT_RECORDS;
		const catalog = parseCapabilityCatalog([...restored]);
		expect(catalog).toHaveLength(36);
		for (let i = 0; i < 36; i++) {
			expect(catalog[i].hashes).toEqual(INSPECT_RECORDS[i].hashes);
		}
	});

	it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
		for (const source of INSPECT_SOURCES) {
			const a = createCapabilityRecord(source);
			const b = createCapabilityRecord(source);
			expect(a.hashes).toEqual(b.hashes);
		}
	});
});
