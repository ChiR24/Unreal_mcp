/**
 * Focused tests: system_control hash parity — recompute and JSON round-trip.
 */
import { describe, expect, it } from 'vitest';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import {
	SYSTEM_CONTROL_RECORDS,
	SYSTEM_CONTROL_SOURCES,
} from './index.js';

describe('system_control hash parity: recompute and JSON round-trip', () => {
	it('every record hash matches a fresh recompute from its source', () => {
		for (let i = 0; i < SYSTEM_CONTROL_SOURCES.length; i++) {
			const recomputed = createCapabilityRecord(SYSTEM_CONTROL_SOURCES[i]);
			expect(recomputed.hashes.schema).toBe(
				SYSTEM_CONTROL_RECORDS[i].hashes.schema,
			);
			expect(recomputed.hashes.content).toBe(
				SYSTEM_CONTROL_RECORDS[i].hashes.content,
			);
		}
	});

	it('JSON round-trip preserves all 55 records with identical hashes', () => {
		const json = JSON.stringify(SYSTEM_CONTROL_RECORDS);
		const restored = JSON.parse(json) as typeof SYSTEM_CONTROL_RECORDS;
		const catalog = parseCapabilityCatalog([...restored]);
		expect(catalog).toHaveLength(55);
		for (let i = 0; i < 55; i++) {
			expect(catalog[i].hashes).toEqual(SYSTEM_CONTROL_RECORDS[i].hashes);
		}
	});

	it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
		for (const source of SYSTEM_CONTROL_SOURCES) {
			const a = createCapabilityRecord(source);
			const b = createCapabilityRecord(source);
			expect(a.hashes).toEqual(b.hashes);
		}
	});
});
