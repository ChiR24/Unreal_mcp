/**
 * Focused tests: system_control security and long-running semantics — command
 * validation, the Python code-size/temp-file guard, read-only effects, and
 * long-running flags.
 */
import { describe, expect, it } from 'vitest';
import { SYSTEM_CONTROL_RECORDS } from './index.js';
import { findByAction } from './system-control-test-helpers.js';

describe('system_control security and long-running semantics', () => {
	it('flags run_ubt, run_tests, run_benchmark, and execute_python as long-running', () => {
		const longRunning = new Set(
			SYSTEM_CONTROL_RECORDS.filter((r) => r.behavior.longRunning).map(
				(r) => r.legacyIds[0].action,
			),
		);
		expect(longRunning).toEqual(
			new Set(['run_ubt', 'run_tests', 'run_benchmark', 'execute_python']),
		);
	});

	it('console_command and execute_command surface command-validation security', () => {
		const consoleCommand = findByAction('console_command');
		const executeCommand = findByAction('execute_command');
		expect(consoleCommand.normalization.rationale.toLowerCase()).toContain(
			'commandvalidator',
		);
		expect(executeCommand.normalization.rationale.toLowerCase()).toContain(
			'commandvalidator',
		);
	});

	it('execute_python surfaces the code-size limit and temp-file scope guard', () => {
		const python = findByAction('execute_python');
		const rationale = python.normalization.rationale.toLowerCase();
		expect(rationale).toContain('1 mb');
		expect(rationale).toContain('temp');
	});

	it('validate_assets and export-adjacent records are read-only validations', () => {
		expect(findByAction('validate_assets').behavior.effect).toBe('read');
		expect(findByAction('get_project_settings').behavior.effect).toBe('read');
		expect(findByAction('get_trace_status').behavior.effect).toBe('read');
		expect(findByAction('analyze_trace').behavior.effect).toBe('read');
	});
});
