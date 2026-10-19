// Fold specs for inspect. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const INSPECT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'inspect_object', selector: 'objectKind',
    summary: 'Deep-inspect an actor or asset, or read a typed detail view of an actor, level, material, texture or mesh (its size, bounds, pivot, LODs and material slots, each with the triangles and bounds of the geometry it covers).',
    topics: ['inspect actor', 'introspect object', 'object properties', 'read all properties', 'actor details', 'level details', 'material details', 'mesh details', 'texture details', 'mesh size', 'mesh dimensions', 'mesh bounds', 'how big is a mesh', 'mesh pivot'],
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
    summary: 'Read one component of an actor or Blueprint: class, relative and world transform, mobility, visibility, attachment, bounds, collision, mesh and chosen property values; info=property returns a single property\'s value.',
    topics: ['bounds and mobility', 'collision and mesh', 'world transform'],
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
    summary: 'Remember or put back an actor\'s transform (location, rotation and scale only) under a snapshot name, in editor memory: create stores it, restore sets it again (transformChanged says whether anything moved). A name used again replaces the old snapshot; snapshots last until the editor closes.',
    members: { create: 'create_snapshot', restore: 'restore_snapshot' },
  },
  {
    primary: 'get_editor_state', selector: 'editorState',
    summary: 'Read editor state: selected actors, viewport, world settings (with whether the game is playing or paused), project settings, editor settings, or focus: what the user is working on (open editors, a Blueprint\'s selected nodes, recent errors).',
    topics: ['selected actors', 'what is selected', 'viewport info', 'world settings', 'project settings', 'editor settings', 'is the game paused', 'what is open', 'selected nodes', 'this node', 'editor focus'],
    members: { selected_actors: 'get_selected_actors', viewport: 'get_viewport_info', world_settings: 'get_world_settings', project_settings: 'get_project_settings', editor_settings: 'get_editor_settings', focus: 'get_editor_focus' },
  },
  {
    primary: 'get_stats', selector: 'statsKind',
    summary: 'Read performance (FPS, frame rate and time, thread and GPU ms), memory usage (used, available, peak), scene statistics (actors by category, components, hidden actors), or frame cost: the thread that bounds the frame and what in view weighs on it (movable shadow-casting lights, heavy meshes, translucency, ticking actors).',
    topics: ['performance stats', 'memory stats', 'scene stats', 'fps', 'game fps', 'measure fps', 'frame rate', 'frame time', 'how fast the game runs', 'gpu time', 'stat unit', 'frame cost', 'why is the game slow', 'performance bottleneck', 'gpu bound'],
    members: { performance: 'get_performance_stats', memory: 'get_memory_stats', scene: 'get_scene_stats', frame: 'get_frame_cost' },
  },
  {
    primary: 'runtime_report',
    summary: 'Report the live state of a running PIE game: the player controller, pawn, view target and camera, plus up to 25 matching actors with class, transform, tags, components and the live values of the variables or properties named in propertyNames; actors are reported only while PIE runs.',
    topics: ['pie state', 'inspect running game', 'live actors', 'player location during play', 'variable value during play', 'runtime value of a variable'],
    members: ['pie_report'],
  },
];
