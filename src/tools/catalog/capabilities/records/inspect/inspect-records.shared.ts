/**
 * Shared fixtures/helpers for the inspect capability record test suite.
 *
 * Reused across the colocated inspect record test modules so the exact 36-action
 * set and the action -> record lookup stay defined in one place.
 */
import { INSPECT_RECORDS } from './index.js';

export const EXPECTED_ACTIONS = [
	'inspect_object',
	'get_actor_details',
	'get_blueprint_details',
	'get_mesh_details',
	'get_texture_details',
	'get_material_details',
	'get_level_details',
	'get_component_details',
	'set_property',
	'get_property',
	'get_components',
	'get_component_property',
	'set_component_property',
	'inspect_class',
	'inspect_cdo',
	'runtime_report',
	'pie_report',
	'list_objects',
	'get_metadata',
	'add_tag',
	'find_by_tag',
	'create_snapshot',
	'restore_snapshot',
	'export',
	'delete_object',
	'find_by_class',
	'get_bounding_box',
	'get_project_settings',
	'get_world_settings',
	'get_viewport_info',
	'get_selected_actors',
	'get_scene_stats',
	'get_performance_stats',
	'get_memory_stats',
	'get_editor_settings',
	'inspect_struct',
] as const;

export function findByAction(action: string) {
	const record = INSPECT_RECORDS.find((r) => r.legacyIds[0].action === action);
	if (!record) throw new Error(`Record not found for action: ${action}`);
	return record;
}
