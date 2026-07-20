/**
 * Availability and normalization-metadata tests for the inspect capability
 * record catalog.
 *
 * Proves: all records target UE 5.0 stable to 5.8 preview 1 with edit state and
 * no plugins, carry inventory-backed normalization class/disposition, and are
 * active with a closed input schema containing the action property. Does not
 * touch the shared core builder, aggregate, pilots, or native code.
 *
 * Per normalization-inventory.json: inspect.get_project_settings is the
 * primary canonical occurrence of cap:shared:get_project_settings (class A,
 * keep) and maps to A_TRUE_DUPLICATE/retain. All other inspect actions are
 * class C with retain disposition.
 */
import { describe, expect, it } from 'vitest';
import { INSPECT_RECORDS } from './index.js';

describe('inspect availability, normalization metadata, and schema closure', () => {
	it('all records target UE 5.0 stable to 5.8 preview 1 with edit state and no plugins', () => {
		for (const record of INSPECT_RECORDS) {
			expect(record.availability.unreal.min).toEqual({
				major: 5,
				minor: 0,
				patch: 0,
				channel: 'stable',
			});
			expect(record.availability.unreal.max).toEqual({
				major: 5,
				minor: 8,
				patch: 0,
				channel: 'preview',
				preview: 1,
			});
			expect(record.availability.editorStates).toEqual(['edit']);
			expect(record.availability.requiredPlugins).toEqual([]);
		}
	});

	it('get_project_settings is A_TRUE_DUPLICATE/retain (inventory primary of cap:shared:get_project_settings)', () => {
		const rec = INSPECT_RECORDS.find(
			(r) => r.legacyIds[0].action === 'get_project_settings',
		);
		if (!rec) throw new Error('get_project_settings record not found');
		expect(rec.normalization.class).toBe('A_TRUE_DUPLICATE');
		expect(rec.normalization.disposition).toBe('retain');
	});

	it('all non-shared records carry C_SAME_VERB_DIFFERENT_TARGET normalization with retain disposition', () => {
		for (const record of INSPECT_RECORDS) {
			const action = record.legacyIds[0].action;
			if (action === 'get_project_settings') continue;
			expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
			expect(record.normalization.disposition).toBe('retain');
			expect(record.normalization.rationale.length).toBeGreaterThan(0);
		}
	});

	it('every record is active with a closed input schema containing the action property', () => {
		for (const record of INSPECT_RECORDS) {
			expect(record.deprecation.status).toBe('active');
			expect(record.schemas.input.additionalProperties).toBe(false);
			const props = record.schemas.input.properties as Record<string, unknown>;
			expect(props).toHaveProperty('action');
			expect(record.schemas.input.required).toContain('action');
		}
	});
});
