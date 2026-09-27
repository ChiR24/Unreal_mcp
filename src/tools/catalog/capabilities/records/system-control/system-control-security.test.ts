/**
 * Focused tests: system_control read-only effects and long-running flags,
 * pinned on the authored (unfolded) records.
 */
import { describe, expect, it } from 'vitest';
import { ALL_UNFOLDED_CAPABILITY_RECORDS } from '../unfolded.js';

const RECORDS = ALL_UNFOLDED_CAPABILITY_RECORDS.filter((record) => record.routing.parentTool === 'system_control');
const effectOf = (action: string) => RECORDS.find((record) => record.legacyIds[0].action === action)?.behavior.effect;

describe('system_control security and long-running semantics', () => {
	it('flags run_ubt, run_tests, run_benchmark, package_project and execute_python as long-running', () => {
		const longRunning = new Set(RECORDS.filter((r) => r.behavior.longRunning).map((r) => r.legacyIds[0].action));
		expect(longRunning).toEqual(new Set(['run_ubt', 'run_tests', 'run_benchmark', 'package_project', 'execute_python']));
	});

	it('validate_assets and export-adjacent records are read-only validations', () => {
		for (const action of ['validate_assets', 'get_project_settings', 'get_trace_status', 'analyze_trace']) {
			expect(effectOf(action), action).toBe('read');
		}
	});
});
