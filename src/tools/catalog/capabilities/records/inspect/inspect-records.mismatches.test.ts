/**
 * Mismatch-surfacing tests for the inspect capability record catalog.
 *
 * Proves: TS/native/action divergences in record metadata (find_by_tag,
 * get_component_details, get_level_details, get_blueprint_details) are surfaced
 * explicitly in the normalization rationale rather than normalized away. Also
 * locks the inventory-backed A_TRUE_DUPLICATE/retain classification for
 * get_project_settings (cap:shared:get_project_settings primary). Does not
 * touch the shared core builder, aggregate, pilots, or native code.
 */
import { describe, expect, it } from 'vitest';
import { findByAction } from './inspect-records.shared.js';

describe('inspect TS/native/action mismatch surfacing (not normalized away)', () => {
	it('find_by_tag surfaces the TS-vs-native routing divergence in its rationale', () => {
		const rec = findByAction('find_by_tag');
		expect(rec.routing.dispatchMode).toBe('action');
		expect(rec.routing.dispatchAction).toBe('control_actor');
		const rationale = rec.normalization.rationale.toLowerCase();
		expect(rationale).toContain('mismatch');
		expect(rationale).toContain('control_actor');
		expect(rationale).toContain('native');
	});

	it('get_component_details surfaces that it dispatches get_components to control_actor', () => {
		const rec = findByAction('get_component_details');
		expect(rec.routing.dispatchMode).toBe('action');
		expect(rec.routing.dispatchAction).toBe('control_actor');
		const rationale = rec.normalization.rationale.toLowerCase();
		expect(rationale).toContain('get_components');
		expect(rationale).toContain('control_actor');
	});

	it('get_level_details surfaces the get_world_settings normalization alias', () => {
		const rec = findByAction('get_level_details');
		expect(rec.routing.dispatchMode).toBe('tool');
		expect(rec.routing.dispatchAction).toBe('get_world_settings');
		expect(rec.normalization.rationale.toLowerCase()).toContain(
			'get_world_settings',
		);
	});

	it('get_blueprint_details surfaces the separate blueprint_get route', () => {
		const rec = findByAction('get_blueprint_details');
		expect(rec.routing.dispatchMode).toBe('action');
		expect(rec.routing.dispatchAction).toBe('blueprint_get');
		expect(rec.normalization.rationale.toLowerCase()).toContain(
			'blueprint_get',
		);
	});

	it('get_project_settings is A_TRUE_DUPLICATE/retain (inventory primary of cap:shared:get_project_settings, not a catalog gap)', () => {
		const rec = findByAction('get_project_settings');
		expect(rec.routing.dispatchMode).toBe('tool');
		expect(rec.routing.dispatchAction).toBe('get_project_settings');
		expect(rec.normalization.class).toBe('A_TRUE_DUPLICATE');
		expect(rec.normalization.disposition).toBe('retain');
		const rationale = rec.normalization.rationale.toLowerCase();
		expect(rationale).toContain('cap:shared:get_project_settings');
		expect(rationale).toContain('system_control');
		expect(rationale).not.toContain('gap');
		expect(rationale).not.toContain('absent');
		expect(rationale).not.toContain('35 of 36');
	});
});
