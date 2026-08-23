/**
 * Shared fixtures/helpers for the focused system_control capability record tests.
 *
 * Extracted only because EXPLICIT_ACTIONS / ALL_55_ACTIONS / findByAction are
 * reused across the split test modules. No production code is touched.
 */
import { PERFORMANCE_ACTIONS } from '../../../../definitions/shared/action-sets.js';
import { SYSTEM_CONTROL_RECORDS } from './index.js';

export const EXPLICIT_ACTIONS = [
	'profile',
	'show_fps',
	'set_quality',
	'screenshot',
	'set_resolution',
	'set_fullscreen',
	'execute_command',
	'console_command',
	'run_ubt',
	'run_tests',
	'subscribe',
	'unsubscribe',
	'spawn_category',
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
	'lumen_update_scene',
	'play_sound',
	'create_widget',
	'show_widget',
	'add_widget_child',
	'set_cvar',
	'get_project_settings',
	'validate_assets',
	'set_project_setting',
	'execute_python',
	'list_plugins',
	'enable_plugin',
	'disable_plugin',
] as const;

export const ALL_55_ACTIONS = [...EXPLICIT_ACTIONS, ...PERFORMANCE_ACTIONS];

export function findByAction(action: string) {
	const record = SYSTEM_CONTROL_RECORDS.find(
		(r) => r.legacyIds[0].action === action,
	);
	if (!record) throw new Error(`Record not found for action: ${action}`);
	return record;
}
