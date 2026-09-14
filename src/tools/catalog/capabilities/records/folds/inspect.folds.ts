// Fold specs for inspect. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const INSPECT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'inspect_object', selector: 'objectKind',
    summary: 'Deep-inspect an actor or asset, or read a typed detail view of an actor, level, material, mesh or texture.',
    topics: ['inspect actor', 'introspect object', 'object properties', 'actor details', 'level details', 'material details', 'mesh details', 'texture details'],
    members: { object: 'inspect_object', actor: 'get_actor_details', level: 'get_level_details', material: 'get_material_details', mesh: 'get_mesh_details', texture: 'get_texture_details' },
  },
  {
    primary: 'inspect_class', selector: 'kind',
    summary: 'Inspect a class, a struct, or a Blueprint\'s class default object.',
    topics: ['class reflection', 'inspect struct', 'class default object', 'cdo'],
    members: { class: 'inspect_class', struct: 'inspect_struct', cdo: 'inspect_cdo' },
  },
  {
    primary: 'get_component_details', selector: 'info',
    summary: 'Read a component\'s details or one of its properties.',
    members: { details: 'get_component_details', property: 'get_component_property' },
  },
  {
    primary: 'query_object', selector: 'lookup',
    summary: 'Query objects: list them, find by tag, read metadata or a bounding box, or export one.',
    topics: ['list objects', 'find by tag', 'metadata', 'bounding box', 'export'],
    members: { list: 'list_objects', by_tag: 'find_by_tag', metadata: 'get_metadata', bounding_box: 'get_bounding_box', export: 'export' },
  },
  {
    primary: 'create_snapshot', selector: 'snapshotOp',
    summary: 'Create a snapshot of an object\'s state, or restore one.',
    members: { create: 'create_snapshot', restore: 'restore_snapshot' },
  },
  {
    primary: 'get_editor_state', selector: 'editorState',
    summary: 'Read editor state: selected actors, viewport, world settings, project settings, editor settings.',
    topics: ['selected actors', 'what is selected', 'viewport info', 'world settings', 'project settings', 'editor settings'],
    members: { selected_actors: 'get_selected_actors', viewport: 'get_viewport_info', world_settings: 'get_world_settings', project_settings: 'get_project_settings', editor_settings: 'get_editor_settings' },
  },
  {
    primary: 'get_stats', selector: 'statsKind',
    summary: 'Read performance, memory or scene statistics.',
    topics: ['performance stats', 'memory stats', 'scene stats', 'fps'],
    members: { performance: 'get_performance_stats', memory: 'get_memory_stats', scene: 'get_scene_stats' },
  },
  { primary: 'runtime_report', summary: 'Report runtime (PIE) object state.', members: ['pie_report'] },
];
