/**
 * Focused tests: system_control normalization metadata - inventory-backed
 * classifications and dispositions for the five true-duplicate actions shared
 * with other parent tools, plus the C-class retain baseline for the rest.
 *
 * Grounded in src/tools/catalog/capabilities/normalization-inventory.json:
 * - cap:shared:console_command  (A/alias; system_control occurrence is alias)
 * - cap:shared:execute_command  (A/alias; system_control occurrence is alias)
 * - cap:shared:get_project_settings (A/alias; system_control occurrence is alias)
 * - cap:shared:screenshot       (A/alias; system_control occurrence is alias)
 * - cap:shared:show_stats       (A/alias; system_control occurrence is alias)
 *
 * Inventory disposition `alias` maps to record disposition `alias`; inventory
 * `keep` on the canonical side maps to `retain` (see control_editor tests).
 */
import { describe, expect, it } from 'vitest';
import { SYSTEM_CONTROL_RECORDS } from './index.js';
import { findByAction } from './system-control-test-helpers.js';

const SHARED_DUPLICATE_ACTIONS = [
	'console_command',
	'execute_command',
	'get_project_settings',
	'screenshot',
	'show_stats',
] as const;

describe('system_control normalization: inventory-backed classifications and dispositions', () => {
	it('the five shared-duplicate actions are A_TRUE_DUPLICATE with alias disposition', () => {
		for (const action of SHARED_DUPLICATE_ACTIONS) {
			const record = findByAction(action);
			expect(record.normalization.class).toBe('A_TRUE_DUPLICATE');
			expect(record.normalization.disposition).toBe('alias');
		}
	});

	it('the five alias rationales name the shared canonical capability and the alias role', () => {
		for (const action of SHARED_DUPLICATE_ACTIONS) {
			const record = findByAction(action);
			const rationale = record.normalization.rationale.toLowerCase();
			expect(rationale).toContain('cap:shared:');
			expect(rationale).toContain('alias');
		}
	});

	it('all remaining system_control actions are C_SAME_VERB_DIFFERENT_TARGET with retain disposition', () => {
		const sharedSet = new Set<string>(SHARED_DUPLICATE_ACTIONS);
		for (const record of SYSTEM_CONTROL_RECORDS) {
			const action = record.legacyIds[0].action;
			if (sharedSet.has(action)) continue;
			expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
			expect(record.normalization.disposition).toBe('retain');
		}
	});

	it('no system_control record claims a nonexistent console_command parent tool', () => {
		for (const record of SYSTEM_CONTROL_RECORDS) {
			const rationale = record.normalization.rationale.toLowerCase();
			expect(rationale).not.toContain('console_command parent');
		}
	});
});
