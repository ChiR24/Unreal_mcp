/**
 * Focused tests: system_control routing — local TS dispatch vs fallback tool
 * dispatch, grounded in the orchestrator routing in
 * consolidated-handler-registration.ts and the native HandleSystemControlAction
 * accept list.
 */
import { describe, expect, it } from 'vitest';
import { PERFORMANCE_ACTIONS } from '../../../../definitions/shared/action-sets.js';
import { SYSTEM_CONTROL_RECORDS } from './index.js';
import { findByAction } from './system-control-test-helpers.js';

describe('system_control routing: local TS dispatch vs fallback tool dispatch', () => {
	it('all records route through the system_control parent tool', () => {
		for (const record of SYSTEM_CONTROL_RECORDS) {
			expect(record.routing.parentTool).toBe('system_control');
		}
	});

	it('only set_project_setting, execute_python and the three plugin actions use fallback tool dispatch to system_control', () => {
		const toolDispatch = SYSTEM_CONTROL_RECORDS.filter(
			(r) => r.routing.dispatchMode === 'tool',
		);
		const toolActions = toolDispatch.map((r) => r.legacyIds[0].action).sort();
		expect(toolActions).toEqual([
			'disable_plugin',
			'enable_plugin',
			'execute_python',
			'list_plugins',
			'set_project_setting',
		]);
		for (const record of toolDispatch) {
			expect(record.routing.dispatchAction).toBe('system_control');
		}
	});

	it('routes console/cvar actions through the local console_command bridge action', () => {
		const consoleActions = [
			'show_fps',
			'profile',
			'set_quality',
			'execute_command',
			'console_command',
			'set_cvar',
			'set_resolution',
			'set_fullscreen',
		];
		for (const action of consoleActions) {
			const record = findByAction(action);
			expect(record.routing.dispatchMode).toBe('local');
			expect(record.routing.dispatchAction).toBe('console_command');
		}
	});

	it('routes widget actions through manage_widget_authoring', () => {
		for (const action of ['create_widget', 'show_widget', 'add_widget_child']) {
			const record = findByAction(action);
			expect(record.routing.dispatchMode).toBe('local');
			expect(record.routing.dispatchAction).toBe('manage_widget_authoring');
		}
	});

	it('routes insights actions through manage_insights and logs through manage_logs', () => {
		const insightsActions = [
			'start_session',
			'start_unreal_insights',
			'capture_insights_trace',
			'get_trace_status',
			'pause_session',
			'resume_session',
			'stop_session',
			'write_snapshot',
			'send_snapshot',
			'analyze_trace',
		];
		for (const action of insightsActions) {
			expect(findByAction(action).routing.dispatchAction).toBe(
				'manage_insights',
			);
		}
		expect(findByAction('subscribe').routing.dispatchAction).toBe(
			'manage_logs',
		);
		expect(findByAction('unsubscribe').routing.dispatchAction).toBe(
			'manage_logs',
		);
		expect(findByAction('spawn_category').routing.dispatchAction).toBe(
			'manage_debug',
		);
		expect(findByAction('run_tests').routing.dispatchAction).toBe(
			'manage_tests',
		);
		expect(findByAction('run_ubt').routing.dispatchAction).toBe(
			'manage_pipeline',
		);
		expect(findByAction('lumen_update_scene').routing.dispatchAction).toBe(
			'manage_render',
		);
		expect(findByAction('play_sound').routing.dispatchAction).toBe(
			'play_sound_2d',
		);
		expect(findByAction('screenshot').routing.dispatchAction).toBe(
			'control_editor',
		);
	});

	it('routes get_project_settings and validate_assets through the local system_control wrapper', () => {
		for (const action of ['get_project_settings', 'validate_assets']) {
			const record = findByAction(action);
			expect(record.routing.dispatchMode).toBe('local');
			expect(record.routing.dispatchAction).toBe('system_control');
		}
	});

	it('routes all performance actions through local TS dispatch', () => {
		for (const action of PERFORMANCE_ACTIONS) {
			expect(findByAction(action).routing.dispatchMode).toBe('local');
		}
		expect(findByAction('enable_gpu_timing').routing.dispatchAction).toBe(
			'manage_performance',
		);
	});
});
