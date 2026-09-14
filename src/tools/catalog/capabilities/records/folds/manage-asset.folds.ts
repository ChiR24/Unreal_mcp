// Fold specs for manage_asset. Data only; see ../shared/fold.ts.
import { byName } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_ASSET_FOLDS: readonly FoldSpec[] = [
  // lifecycle (action-mode writes)
  {
    primary: 'import_marketplace_asset', selector: 'marketplace',
    summary: 'Bring a marketplace asset into the project: add a Fab listing, download a Fab asset, or import a Megascans asset.',
    topics: ['fab', 'megascans', 'marketplace', 'download asset', 'quixel'],
    members: { fab_listing: 'add_fab_asset_to_project', fab_download: 'download_fab_asset', megascans: 'import_megascans_asset' },
  },
  {
    primary: 'source_control', selector: 'sourceControlOp',
    summary: 'Source control operations on assets: check out or submit.',
    topics: ['source control', 'checkout', 'submit', 'perforce', 'git'],
    members: { checkout: 'source_control_checkout', submit: 'source_control_submit' },
  },
  {
    primary: 'set_metadata', selector: 'kind',
    summary: 'Set asset metadata key/values or asset tags.',
    members: { metadata: 'set_metadata', tags: 'set_tags' },
  },
  {
    primary: 'maintain_content', selector: 'maintenance',
    summary: 'Content maintenance: bulk-rename assets, fix up redirectors, or migrate packages from another content source.',
    topics: ['bulk rename', 'fixup redirectors', 'migrate assets', 'content maintenance'],
    members: { bulk_rename: 'bulk_rename', fixup_redirectors: 'fixup_redirectors', migrate: 'migrate_assets' },
  },
  {
    primary: 'edit_material_instance', selector: 'edit',
    summary: 'Edit a material instance: add a parameter override or reset all instance parameters.',
    topics: ['material instance parameter', 'reset instance parameters'],
    members: { add_parameter: 'add_material_parameter', reset_parameters: 'reset_instance_parameters' },
  },
  // lifecycle (action-mode reads)
  {
    primary: 'query_marketplace', selector: 'lookup',
    summary: 'Query marketplace libraries: Fab listing details, Fab downloads, Fab library, Fab search, Megascans library.',
    topics: ['fab library', 'fab listing', 'search fab', 'megascans library', 'marketplace'],
    members: {
      fab_listing_details: 'get_fab_listing_details', fab_downloads: 'list_fab_downloads', fab_library: 'list_fab_library',
      fab_search: 'search_fab_listings', megascans_library: 'list_megascans_library',
    },
  },
  {
    primary: 'list', selector: 'kind',
    summary: 'List assets under a path, the registered content sources, or the instances of a material.',
    members: { assets: 'list', content_sources: 'list_content_sources', material_instances: 'list_instances' },
  },
  {
    primary: 'query_asset', selector: 'lookup',
    summary: 'Query assets: existence, search by text/class, find by tag, reference graph analysis, material stats, source-control state.',
    topics: ['find assets', 'search assets', 'asset exists', 'find by tag', 'find assets by tag', 'analyze graph', 'material stats', 'source control state'],
    members: {
      exists: 'exists', search: 'search_assets', by_tag: 'find_by_tag', graph: 'analyze_graph',
      material_stats: 'get_material_stats', source_control_state: 'get_source_control_state',
    },
  },
  // lifecycle (tool-mode)
  { primary: 'duplicate', summary: 'Duplicate an asset to a new path or name.', members: ['duplicate_asset'] },
  { primary: 'move', summary: 'Move an asset to a new path.', members: ['move_asset'] },
  { primary: 'rename', summary: 'Rename an asset.', members: ['rename_asset'] },
  {
    primary: 'delete', summary: 'Delete one or more assets.',
    topics: ['delete asset', 'delete assets', 'remove asset', 'delete asset permanently', 'delete imported asset'],
    members: ['delete_asset', 'delete_assets'],
  },
  {
    primary: 'process_asset', selector: 'process',
    summary: 'Post-process an asset: render a thumbnail or generate mesh LODs.',
    topics: ['thumbnail', 'generate lods'],
    members: { thumbnail: 'create_thumbnail', lods: 'generate_lods' },
  },
  {
    primary: 'inspect_asset', selector: 'lookup',
    summary: 'Inspect an asset: metadata, dependencies, reference graph, validation, or a directory report.',
    topics: ['asset metadata', 'asset dependencies', 'asset graph', 'validate asset', 'asset report'],
    members: { metadata: 'get_metadata', dependencies: 'get_dependencies', graph: 'get_asset_graph', validate: 'validate', report: 'generate_report' },
  },
  // datatable
  {
    primary: 'edit_data_table', selector: 'edit',
    summary: 'Create a data table or row struct, add/update/import rows, or set the row struct.',
    topics: ['data table', 'data table row', 'row struct', 'import rows'],
    members: {
      create: 'create_data_table', create_row_struct: 'create_row_struct', add_row: 'add_data_table_row', update_row: 'update_data_table_row',
      import_rows: 'import_data_table_rows', set_row_struct: 'set_data_table_row_struct', set_struct_as_row_struct: 'set_struct_as_row_struct',
    },
  },
  {
    primary: 'delete_data_table_row', selector: 'deleteScope',
    summary: 'Delete one data table row, or clear every row.',
    members: { row: 'delete_data_table_row', all: 'clear_data_table_rows' },
  },
  {
    primary: 'inspect_data_table', selector: 'info',
    summary: 'Read a data table: one row, all rows, or its row struct.',
    topics: ['data table row', 'list rows', 'row struct'],
    members: { row: 'get_data_table_row', rows: 'list_data_table_rows', row_struct: 'get_row_struct' },
  },
  // enum
  {
    primary: 'edit_enum', selector: 'edit',
    summary: 'Create an enum or edit its values: add, rename, reorder, set metadata, split.',
    topics: ['enum value', 'user defined enum', 'reorder enum', 'split enum'],
    members: { create: 'create_enum', add_value: 'add_enum_value', rename_value: 'rename_enum_value', reorder_values: 'reorder_enum_values', set_value_metadata: 'set_enum_value_metadata', split: 'split_enum' },
  },
  {
    primary: 'delete_enum', selector: 'deleteScope',
    summary: 'Delete an enum asset or remove one of its values.',
    members: { enum: 'delete_enum', value: 'remove_enum_value' },
  },
  // material authoring
  {
    primary: 'add_material_node', selector: 'nodeKind',
    summary: 'Add a node to a material graph: any expression class by name, or a typed node (parameters, texture sample/coordinate, math, noise, panner, rotator, fresnel, switches, custom HLSL, function call, landscape layer).',
    topics: ['material node', 'material expression', 'scalar parameter', 'vector parameter', 'texture sample', 'material function', 'custom expression', 'landscape layer'],
    members: {
      node: 'add_material_node', custom_expression: 'add_custom_expression', fresnel: 'add_fresnel', if: 'add_if', math: 'add_math_node',
      noise: 'add_noise', panner: 'add_panner', pixel_depth: 'add_pixel_depth', reflection_vector: 'add_reflection_vector', rotator: 'add_rotator',
      scalar_parameter: 'add_scalar_parameter', static_switch_parameter: 'add_static_switch_parameter', switch: 'add_switch',
      texture_coordinate: 'add_texture_coordinate', texture_sample: 'add_texture_sample', vector_parameter: 'add_vector_parameter',
      vertex_normal: 'add_vertex_normal', voronoi: 'add_voronoi', world_position: 'add_world_position',
      material_function: 'use_material_function', landscape_layer: 'add_landscape_layer',
    },
  },
  {
    primary: 'add_function_io', selector: 'io',
    summary: 'Add an input or output pin to a material function.',
    topics: ['material function input', 'material function output'],
    members: { input: 'add_function_input', output: 'add_function_output' },
  },
  {
    primary: 'create_material', selector: 'kind',
    summary: 'Create a material asset: a standard material, a decal, landscape or post-process material, or a material function.',
    topics: ['create material', 'decal material', 'landscape material', 'post process material', 'material function'],
    members: { material: 'create_material', decal: 'create_decal_material', landscape: 'create_landscape_material', post_process: 'create_post_process_material', function: 'create_material_function' },
  },
  { primary: 'connect_nodes', summary: 'Connect two material graph pins.', members: ['connect_material_pins'] },
  { primary: 'disconnect_nodes', summary: 'Disconnect a material graph pin or node.', members: ['break_material_connections'] },
  { primary: 'delete_node', summary: 'Delete a node from a material graph.', members: ['remove_material_node'] },
  {
    primary: 'compile_material', selector: 'compileOp',
    summary: 'Compile a material, or rebuild it from its graph.',
    members: { compile: 'compile_material', rebuild: 'rebuild_material' },
  },
  {
    primary: 'set_material_property', selector: 'materialProperty',
    summary: 'Set a material property: blend mode, domain, shading model or two-sided.',
    topics: ['blend mode', 'material domain', 'shading model', 'two sided'],
    members: { blend_mode: 'set_blend_mode', domain: 'set_material_domain', shading_model: 'set_shading_model', two_sided: 'set_two_sided' },
  },
  {
    primary: 'set_material_parameter', selector: 'parameterKind',
    summary: 'Set a material or instance parameter value: any parameter by type, or a scalar, vector, texture or static-switch parameter.',
    topics: ['material parameter', 'scalar parameter value', 'vector parameter value', 'texture parameter', 'static switch'],
    members: { parameter: 'set_material_parameter', scalar: 'set_scalar_parameter_value', vector: 'set_vector_parameter_value', texture: 'set_texture_parameter_value', static_switch: 'set_static_switch_parameter_value' },
  },
  {
    primary: 'get_material_info', selector: 'info',
    summary: 'Inspect a material or material function: summary, node lookup, node details/properties/connections, node chains and connected subgraphs.',
    topics: ['material info', 'material node', 'node connections', 'find node', 'material function info'],
    members: {
      material: 'get_material_info', function: 'get_material_function_info', find_node: 'find_node', node_details: 'get_material_node_details',
      node_properties: 'get_node_properties', node_connections: 'get_node_connections', node_chain: 'get_node_chain', subgraph: 'get_connected_subgraph',
    },
  },
  // struct
  {
    primary: 'edit_struct', selector: 'edit',
    summary: 'Create a user-defined struct or edit it: members, defaults, metadata, types, order, rename, duplicate, import, recompile, refresh dependencies, instanced struct properties.',
    topics: ['user defined struct', 'struct member', 'struct default', 'recompile struct', 'instanced struct'],
    members: byName(['create_struct', 'add_struct_member', 'rename_struct_member', 'reorder_struct_members', 'set_struct_member_default',
      'set_struct_member_metadata', 'set_struct_member_type', 'rename_struct', 'duplicate_struct', 'import_struct', 'recompile_struct',
      'refresh_struct_dependencies', 'set_instanced_struct_property']),
  },
  {
    primary: 'get_struct', selector: 'info',
    summary: 'Read a struct: definition, members, usage, comparison, export, instanced property values, or list structs.',
    topics: ['struct members', 'list structs', 'compare structs', 'export struct', 'struct usage'],
    members: {
      struct: 'get_struct', read: 'read_struct', members: 'list_struct_members', list: 'list_structs', usage: 'search_struct_usage',
      compare: 'compare_structs', export: 'export_struct', instanced_property: 'get_instanced_struct_property',
    },
  },
  {
    primary: 'delete_struct', selector: 'deleteScope',
    summary: 'Delete a struct asset or remove one of its members.',
    members: { struct: 'delete_struct', member: 'remove_struct_member' },
  },
  // texture
  {
    primary: 'create_texture', selector: 'kind',
    summary: 'Create a texture: gradient, noise, pattern, AO from mesh, normal from height, resized copy, channel pack/extract, or a combination of two textures.',
    topics: ['create texture', 'noise texture', 'gradient texture', 'normal map', 'channel pack', 'resize texture', 'combine textures'],
    members: {
      gradient: 'create_gradient_texture', noise: 'create_noise_texture', pattern: 'create_pattern_texture', ao_from_mesh: 'create_ao_from_mesh',
      normal_from_height: 'create_normal_from_height', resized: 'resize_texture', channel_pack: 'channel_pack', channel_extract: 'channel_extract', combined: 'combine_textures',
    },
  },
  {
    primary: 'adjust_texture', selector: 'adjust',
    summary: 'Adjust texture pixels in place: curves, levels, blur, sharpen, desaturate, invert.',
    topics: ['adjust texture', 'blur texture', 'sharpen', 'desaturate', 'invert texture', 'levels', 'curves'],
    members: { curves: 'adjust_curves', levels: 'adjust_levels', blur: 'blur', sharpen: 'sharpen', desaturate: 'desaturate', invert: 'invert' },
  },
  {
    primary: 'configure_texture', selector: 'setting',
    summary: 'Configure texture settings: compression, LOD bias, streaming priority, texture group, virtual texturing.',
    topics: ['texture compression', 'lod bias', 'streaming priority', 'texture group', 'virtual texture'],
    members: { compression: 'set_compression_settings', lod_bias: 'set_lod_bias', streaming_priority: 'set_streaming_priority', texture_group: 'set_texture_group', virtual_texture: 'configure_virtual_texture' },
  },
];
