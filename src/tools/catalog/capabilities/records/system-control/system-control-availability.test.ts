/**
 * Focused tests: system_control availability — UE 5.0 stable to 5.8 preview 1
 * in the edit state, plus the PythonScriptPlugin requirement on execute_python.
 */
import { describe, expect, it } from 'vitest';
import { SYSTEM_CONTROL_RECORDS } from './index.js';
import { findByAction } from './system-control-test-helpers.js';

describe('system_control availability', () => {
	it('all records target UE 5.0 stable to 5.8 preview 1 in the edit state', () => {
		for (const record of SYSTEM_CONTROL_RECORDS) {
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
		}
	});

	it('execute_python requires the PythonScriptPlugin', () => {
		expect(
			findByAction('execute_python').availability.requiredPlugins,
		).toContain('PythonScriptPlugin');
	});
});
