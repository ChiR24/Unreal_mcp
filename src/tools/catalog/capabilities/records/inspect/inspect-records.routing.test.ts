/**
 * Routing-fidelity tests for the inspect capability record catalog.
 *
 * Proves: every record routes through the inspect parent tool, tool-mode
 * dispatch carries the dispatched sub-action, and action-mode dispatch targets
 * control_actor or blueprint_get. Does not touch the shared core builder,
 * aggregate, pilots, or native code.
 */
import { describe, expect, it } from 'vitest';
import { INSPECT_RECORDS } from './index.js';
import { findByAction } from './inspect-records.shared.js';

describe('inspect routing fidelity to TS handlers and native Inspect dispatch', () => {
	it('routes all records through the inspect parent tool', () => {
		for (const record of INSPECT_RECORDS) {
			expect(record.routing.parentTool).toBe('inspect');
		}
	});

	it('inspect-dispatched actions use tool mode with the dispatched sub-action', () => {
		const toolRouted: Record<string, string> = {
			inspect_object: 'inspect_object',
			get_actor_details: 'inspect_object',
			get_mesh_details: 'inspect_object',
			get_texture_details: 'inspect_object',
			get_material_details: 'inspect_object',
			get_level_details: 'get_world_settings',
			get_property: 'get_property',
			set_property: 'set_property',
			get_components: 'get_components',
			inspect_class: 'inspect_class',
			inspect_cdo: 'inspect_cdo',
			runtime_report: 'runtime_report',
			pie_report: 'pie_report',
			find_by_class: 'find_by_class',
			get_project_settings: 'get_project_settings',
			get_world_settings: 'get_world_settings',
			get_viewport_info: 'get_viewport_info',
			get_selected_actors: 'get_selected_actors',
			get_scene_stats: 'get_scene_stats',
			get_performance_stats: 'get_performance_stats',
			get_memory_stats: 'get_memory_stats',
			get_editor_settings: 'get_editor_settings',
			inspect_struct: 'inspect_struct',
		};
		expect(Object.keys(toolRouted).length).toBe(23);
		for (const [action, dispatchAction] of Object.entries(toolRouted)) {
			const record = findByAction(action);
			expect(record.routing.dispatchMode).toBe('tool');
			expect(record.routing.dispatchAction).toBe(dispatchAction);
		}
	});

	it('cross-route actions dispatch to control_actor or blueprint_get', () => {
		const actionRouted: Record<string, string> = {
			get_blueprint_details: 'blueprint_get',
			get_component_details: 'control_actor',
			get_component_property: 'control_actor',
			set_component_property: 'control_actor',
			get_metadata: 'control_actor',
			add_tag: 'control_actor',
			find_by_tag: 'control_actor',
			create_snapshot: 'control_actor',
			restore_snapshot: 'control_actor',
			export: 'control_actor',
			delete_object: 'control_actor',
			list_objects: 'control_actor',
			get_bounding_box: 'control_actor',
		};
		expect(Object.keys(actionRouted).length).toBe(13);
		for (const [action, dispatchAction] of Object.entries(actionRouted)) {
			const record = findByAction(action);
			expect(record.routing.dispatchMode).toBe('action');
			expect(record.routing.dispatchAction).toBe(dispatchAction);
		}
	});
});
