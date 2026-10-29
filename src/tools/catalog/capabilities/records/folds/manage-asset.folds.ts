// Fold specs for manage_asset. Data only; see ../shared/fold.ts.
import { byName } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_ASSET_FOLDS: readonly FoldSpec[] = [
  // lifecycle (action-mode writes)
  {
    primary: 'import_marketplace_asset', selector: 'marketplace',
    summary: 'Bring a marketplace asset into the project: add a Fab listing (it returns at once with an operationId; poll query_marketplace lookup=fab_import_status), cancel a Fab import that is queued or downloading, download a Fab asset, or import a Megascans asset.',
    topics: ['fab', 'megascans', 'marketplace', 'download asset', 'quixel', 'cancel fab import', 'stop fab import'],
    members: { fab_listing: 'add_fab_asset_to_project', fab_cancel: 'cancel_fab_import', fab_download: 'download_fab_asset', megascans: 'import_megascans_asset' },
  },
  {
    primary: 'source_control', selector: 'sourceControlOp',
    summary: 'Revision control for the project, chosen by sourceControlOp: init makes a Git repository at the project root with an Unreal .gitignore and a first commit and turns the Git provider on; commit_all stages and commits every change (alreadyClean when there is none); enable turns a provider on or switches to it (Git, Perforce, Subversion); checkout and submit check assets out and in through the provider that is on, listing paths that name no asset (skipped). Git runs on a worker and every step comes back with its output.',
    topics: ['source control', 'revision control', 'checkout', 'submit', 'commit', 'init repository', 'perforce', 'git', 'check out file', 'initialize git repo', 'connect to perforce', 'set up version control', 'check in changes'],
    members: {
      checkout: 'source_control_checkout', submit: 'source_control_submit',
      enable: 'source_control_enable', init: 'source_control_init',
      commit_all: 'source_control_commit_all',
    },
  },
  {
    primary: 'set_metadata', topics: ['tag asset', 'tag an asset', 'asset metadata', 'add asset tag'], selector: 'kind',
    summary: 'Write package metadata on an asset (kind metadata: key/value pairs, numbers and booleans as text, objects and arrays as JSON text, null skipped; updatedKeys counts them) or tag it (tags: each tag becomes a metadata key set to true; appliedTags). The asset is marked unsaved, not saved (save_all keeps it). Package metadata is editor-only: cooking strips it, so the running game cannot read it.',
    members: { metadata: 'set_metadata', tags: 'set_tags' },
  },
  {
    primary: 'maintain_content', selector: 'maintenance',
    summary: 'Content maintenance: bulk-rename assets, fix up redirectors, refresh the nodes of every Blueprint in a folder, or migrate packages from another content source.',
    topics: ['bulk rename', 'fixup redirectors', 'migrate assets', 'content maintenance', 'refresh all nodes', 'refresh blueprints', 'fix redirectors', 'rename multiple assets', 'batch rename', 'clean up the project'],
    members: { bulk_rename: 'bulk_rename', fixup_redirectors: 'fixup_redirectors', refresh_blueprints: 'refresh_blueprints', migrate: 'migrate_assets' },
  },
  {
    primary: 'edit_material_instance', selector: 'edit',
    // add_parameter adds a parameter EXPRESSION to a material graph, so it needs a
    // Material or Material Function; a material instance has no graph and is refused.
    // Only reset_parameters takes an instance. The old summary promised instance
    // parameter overrides from both, which is what set_material_parameter does.
    summary: 'Add a parameter expression to a material or material function, or reset every parameter override on a material instance. To override a parameter value on an instance, use set_material_parameter.',
    topics: ['material parameter expression', 'reset instance parameters'],
    members: { add_parameter: 'add_material_parameter', reset_parameters: 'reset_instance_parameters' },
  },
  // lifecycle (action-mode reads)
  {
    primary: 'query_marketplace', selector: 'lookup',
    summary: 'Query marketplace libraries: Fab listing details, Fab downloads, Fab library, Fab search, Megascans library, and the status of a Fab import that is running.',
    topics: ['fab library', 'fab listing', 'search fab', 'megascans library', 'marketplace', 'my fab downloads', 'browse marketplace', 'fab import status', 'is the fab import done'],
    members: {
      fab_listing_details: 'get_fab_listing_details', fab_downloads: 'list_fab_downloads', fab_library: 'list_fab_library',
      fab_search: 'search_fab_listings', megascans_library: 'list_megascans_library', fab_import_status: 'get_fab_import_status',
    },
  },
  {
    primary: 'list', selector: 'kind',
    summary: 'List the assets and subfolders in a folder (path), the registered content sources, or the instances of a material.',
    topics: ['list assets', 'browse folder', 'content browser', 'assets in folder', 'directory listing', 'folder contents', 'what is in this folder', 'show folders', 'count assets in folder', 'get assets in folder'],
    whenToUse: [
      'kind=assets: what a folder holds must be seen (subfolders included unless recursive is false).',
      'kind=content_sources: which template, feature pack, plugin or Fab folders exist to copy content from.',
      'kind=material_instances: which material instances use a parent material.',
    ],
    whenNotToUse: ['An asset must be found by name, class or tag anywhere in the project (use query_asset).'],
    members: { assets: 'list', content_sources: 'list_content_sources', material_instances: 'list_instances' },
  },
  {
    primary: 'query_asset', selector: 'lookup',
    summary: 'Query assets: existence, search by name/class, find by tag, find where a text appears inside assets (graph nodes by title too, so every call of a function), the node graph inside a material or Blueprint, material stats, source-control state.',
    topics: ['find assets', 'search assets', 'asset exists', 'find by tag', 'find assets by tag', 'find text', 'where is text used', 'where is function called', 'find function calls', 'analyze graph', 'material stats', 'source control state', 'list all widget blueprints', 'asset name', 'look up asset', 'project assets', 'list all meshes', 'is asset checked out'],
    members: {
      exists: 'exists', search: 'search_assets', by_tag: 'find_by_tag', text: 'find_text', graph: 'analyze_graph',
      material_stats: 'get_material_stats', source_control_state: 'get_source_control_state',
    },
  },
  // lifecycle (tool-mode)
  { primary: 'duplicate', summary: 'Copy an asset, or a whole folder with its sub-folders, and save the copies. destinationPath is a folder (the copy keeps the source name, or takes newName), a full asset path, or a bare name beside the source; newName alone copies beside the source. A taken destination is refused (DESTINATION_EXISTS) and a missing source answers ASSET_NOT_FOUND; a folder copy skips assets whose destination is taken (duplicatedCount under assetCount). Each asset is copied on its own: what referred to the original, placed actors and the other copies included, still does. A copied Blueprint gets its Math Expressions rebound and a copied Niagara system is compiled, so both work at once.', members: ['duplicate_asset'] },
  {
    primary: 'move',
    summary: 'Move an asset to a new path, or a whole folder: every asset under it moves (sub-folders kept) and references, redirectors and project settings follow.',
    topics: ['move folder', 'move content folder', 'relocate assets', 'organize assets', 'put assets in folder'],
    members: ['move_asset'],
  },
  {
    primary: 'rename',
    summary: 'Rename an asset or a folder in place (newName), or move it (destinationPath); a folder takes every asset under it along. Several unrelated assets in one call: maintain_content bulk_rename with renames.',
    topics: ['rename folder', 'rename several assets', 'rename material', 'rename texture', 'rename blueprint'],
    members: ['rename_asset'],
  },
  {
    primary: 'delete', summary: 'Delete one or more assets, or whole folders. A folder of thousands of assets can take minutes and reports how far it has got as it goes; when the call answers before the delete is done (task.state running), the delete carries on, so check the folder with asset.list instead of deleting again.',
    topics: ['delete asset', 'delete assets', 'remove asset', 'delete asset permanently', 'delete imported asset', 'delete texture', 'delete folder', 'delete mesh', 'delete asset from project'],
    members: ['delete_asset', 'delete_assets'],
  },
  {
    primary: 'process_asset', selector: 'process',
    summary: 'Process an asset, chosen by process. thumbnail renders its preview at width by height (default 512, 1 to 4096) and stores it in the asset, which is left unsaved (thumbnailCached; engine content keeps its own), writing it as a PNG as well when outputPath names a project-relative file. lods gives static meshes lodCount LODs in all (default 4, LOD 0 included, each half the triangles of the one before), replacing LODs past the count and hand-tuned settings, then rebuilds and saves each. mesh_materials sets the materials in the slots of a static or skeletal mesh asset, by slot index or name, so every placement shows them; an engine mesh changes for this session only and is never saved (saveSkippedReason). mesh_collision sets the collision of a static mesh asset (a fitted box, sphere or capsule, its render triangles, or none), rebuilds every placed copy and saves it. lods and mesh_collision refuse engine meshes (ENGINE_CONTENT): duplicate one into /Game first.',
    topics: ['thumbnail', 'generate lods', 'material thumbnail', 'mesh collision', 'static mesh collision'],
    members: { thumbnail: 'create_thumbnail', lods: 'generate_lods', mesh_materials: 'set_mesh_materials', mesh_collision: 'set_mesh_collision' },
  },
  {
    primary: 'inspect_asset', selector: 'lookup',
    summary: 'Inspect an asset: metadata, what it uses or what uses it (referencers), its dependency graph, an exists-and-loads check, a directory report (every asset under a folder, or with reportType unreferenced only those nothing uses), or a diff: its unsaved changes against the saved file, or a compare with another asset.',
    topics: ['asset metadata', 'asset dependencies', 'asset referencers', 'what uses this asset', 'find references', 'asset graph', 'validate asset', 'asset report', 'asset used', 'show asset details', 'look at asset', 'meshes using a material', 'which assets use this', 'what uses this material', 'unsaved changes', 'compare assets'],
    members: { metadata: 'get_metadata', dependencies: 'get_dependencies', graph: 'get_asset_graph', validate: 'validate', report: 'generate_report', diff: 'diff_asset' },
  },
  // datatable
  {
    primary: 'edit_data_table', selector: 'edit',
    summary: 'Write a data table, chosen by edit. create makes a table (name and path, default /Game/DataTables, or dataTablePath) bound to the row struct at rowStructPath; create_row_struct makes a struct for its rows (name and path, default /Game/Structs, or rowStructPath) from members. add_row adds rowName with rowData, the columns it leaves out taking the struct defaults, and refuses a name already there (ROW_ALREADY_EXISTS); update_row changes only the columns rowData names in an existing row; import_rows loads rows of {rowName, rowData} in one call, each named row replaced whole (clearExisting drops every row first), and lists the rows that do not fit under invalidRows. set_row_struct (set_struct_as_row_struct is the same) binds another row struct and carries the rows over (migrateExistingRows, default true): a row whose values do not convert is dropped and listed under invalidRows, and a filled table is refused unless the rows are migrated or cleared. rowData keys are the struct property names, as inspect_data_table reads them back. Every edit saves the asset unless save is false (saved).',
    topics: ['data table', 'data table row', 'row struct', 'import rows', 'make data table', 'change data table row', 'add item to data table'],
    members: {
      create: 'create_data_table', create_row_struct: 'create_row_struct', add_row: 'add_data_table_row', update_row: 'update_data_table_row',
      import_rows: 'import_data_table_rows', set_row_struct: 'set_data_table_row_struct', set_struct_as_row_struct: 'set_struct_as_row_struct',
    },
  },
  {
    primary: 'delete_data_table_row', selector: 'deleteScope',
    summary: 'Delete one data table row by rowName (deleteScope row, the default; ROW_NOT_FOUND names a missing one) or every row (all; rowsCleared counts them), and save the table unless save is false (saved says whether it was written). Logic that looks a row up by name gets nothing back for a deleted one.',
    members: { row: 'delete_data_table_row', all: 'clear_data_table_rows' },
  },
  {
    primary: 'inspect_data_table', selector: 'info',
    summary: 'Read a data table at dataTablePath (ASSET_NOT_FOUND when none is there). info row, the default, reads one row by rowName: found, and its rowData, each column under its property name in the form edit_data_table writes; a missing row answers found false rather than an error. rows lists every row with its values (rows of rowName and rowData), with rowCount the table total and rowStructPath; a listing too large for one reply comes a page at a time (send page.nextCall for the next), and past 10,000 rows truncated is true. row_struct answers hasRowStruct and rowStructPath (struct.get_struct reads the columns).',
    topics: ['data table row', 'list rows', 'row struct', 'look at data table', 'view data table rows'],
    members: { row: 'get_data_table_row', rows: 'list_data_table_rows', row_struct: 'get_row_struct' },
  },
  // enum
  {
    primary: 'edit_enum', selector: 'edit',
    summary: 'Create a user enum or edit its values, chosen by edit. create makes it (name and path, default /Game/Enums, or enumPath) with values in order; a name given twice is refused and nothing is made. add_value appends valueName (index and valueCount answer where). rename_value changes the name the editor shows and keeps the internal name every stored value (defaults, pins, table cells) holds, answered as internalName. reorder_values takes order, every value once, and moves them as the enum editor does, so stored values keep their entries (the order it already has changes nothing). set_value_metadata sets key to value on one entry, such as its ToolTip. split copies the named values, or every value from index on, into a new enum newEnumName (in path, default /Game/Enums) and leaves the source as it is. A value is named by its internal or its shown name; a name the enum already has is VALUE_EXISTS and one it lacks NOT_FOUND. Every edit saves unless save is false.',
    topics: ['enum value', 'user defined enum', 'reorder enum', 'split enum', 'make enum', 'new enum', 'define enum'],
    members: { create: 'create_enum', add_value: 'add_enum_value', rename_value: 'rename_enum_value', reorder_values: 'reorder_enum_values', set_value_metadata: 'set_enum_value_metadata', split: 'split_enum' },
  },
  {
    primary: 'delete_enum', selector: 'deleteScope',
    summary: 'Delete an enum asset (deleteScope enum, the default; refused with DELETE_FAILED, naming referencedBy, while anything still uses it) or remove one of its values (value: the later values move up one while loaded Blueprints and assets holding them keep their entries, remapped by name as the enum editor does, and pins set to the removed one are reset to the first entry). Saved unless save is false.',
    members: { enum: 'delete_enum', value: 'remove_enum_value' },
  },
  // material authoring
  {
    primary: 'add_material_node', selector: 'nodeKind',
    summary: 'Add a node to a material graph: any expression class by name, or a typed node (parameters, texture sample/coordinate, math, noise, panner, rotator, fresnel, switches, custom HLSL, function call, landscape layer), or build a whole graph (nodes, wires, material properties) in one batch.',
    topics: ['material node', 'material expression', 'scalar parameter', 'vector parameter', 'texture sample', 'material function', 'custom expression', 'landscape layer', 'build material graph', 'create custom shader'],
    members: {
      node: 'add_material_node', custom_expression: 'add_custom_expression', fresnel: 'add_fresnel', if: 'add_if', math: 'add_math_node',
      noise: 'add_noise', panner: 'add_panner', pixel_depth: 'add_pixel_depth', reflection_vector: 'add_reflection_vector', rotator: 'add_rotator',
      scalar_parameter: 'add_scalar_parameter', static_switch_parameter: 'add_static_switch_parameter', switch: 'add_switch',
      texture_coordinate: 'add_texture_coordinate', texture_sample: 'add_texture_sample', vector_parameter: 'add_vector_parameter',
      vertex_normal: 'add_vertex_normal', voronoi: 'add_voronoi', world_position: 'add_world_position',
      material_function: 'use_material_function', landscape_layer: 'add_landscape_layer',
      batch: 'build_material_graph',
    },
  },
  {
    primary: 'add_function_io', selector: 'io',
    summary: 'Add a pin to a material function (assetPath), chosen by io: input adds a Function Input named inputName of inputType (default Vector3; an unknown type is refused) that callers wire into; output adds a Function Output named inputName that carries out whatever is wired into it, of that type. A name the function already has is NAME_TAKEN. The node lands at x, y (default 0, 0), the function recompiles and is left unsaved; the reply gives its nodeId for connect_nodes.',
    topics: ['material function input', 'material function output'],
    members: { input: 'add_function_input', output: 'add_function_output' },
  },
  {
    primary: 'create_material', selector: 'kind',
    summary: 'Create a material asset: a standard material, a decal, landscape or post-process material, a material function, or a Material Parameter Collection (values every material reading it shares).',
    topics: ['create material', 'decal material', 'landscape material', 'post process material', 'material function', 'material parameter collection', 'emissive glow', 'glowing material', 'shiny metal material', 'make a cube or a wall red'],
    members: { material: 'create_material', decal: 'create_decal_material', landscape: 'create_landscape_material', post_process: 'create_post_process_material', function: 'create_material_function', parameter_collection: 'create_parameter_collection' },
  },
  { primary: 'connect_nodes', topics: ['connect texture to base color', 'connect node to base color'], summary: 'Wire an output of sourceNodeId (sourcePin: a name, an index or channel letters such as G or RG; default its first output) into targetPin of targetNodeId, replacing whatever fed that input. targetNodeId Main is the material output (BaseColor, Roughness, Normal, EmissiveColor...), or in a material function its output named targetPin (the first when left out). The material recompiles and is left unsaved (save_all keeps it). NODE_NOT_FOUND names Main when the target id is missing; INVALID_PIN or PIN_NOT_FOUND lists the inputs the node has.', members: ['connect_material_pins'] },
  { primary: 'disconnect_nodes', summary: 'Unplug the wire into pinName of nodeId (or with nodeId Main a material output such as BaseColor), keeping both nodes; the input falls back to its default. A pin name that matches none fails listing the inputs; a material function output node with no pinName unplugs every output. The material recompiles and is left unsaved (save_all keeps it).', members: ['break_material_connections'] },
  { primary: 'delete_node', summary: 'Delete nodes from a material or material function graph: nodeId, or nodeIds for several. Every wire to a deleted node is cut, the material outputs it fed included, so what it fed falls back to its default; the material is then recompiled and saved unless save is false. Ids that match nothing come back in notFound, and NOT_FOUND answers only when none matched (read the ids with get_material_info or find_node).', members: ['remove_material_node'] },
  {
    primary: 'compile_material', selector: 'compileOp',
    summary: 'Recompile a material or material function from its graph (compileOp compile, the default; rebuild runs the same recompile) and wait for every shader. A material that does not compile answers compiled false with compileErrors, since it renders as the default material until fixed; saved says whether it was written (save, default true). A material function is updated without the shader check.',
    members: { compile: 'compile_material', rebuild: 'rebuild_material' },
  },
  {
    primary: 'set_material_property', selector: 'materialProperty',
    summary: 'Set a property of a material, or override it on a material instance, by materialProperty: blend_mode, domain, shading_model or two_sided. The material recompiles and is saved unless save is false (saved), and the reply gives the value written; on an instance the value becomes an override of its parent, as its Material Property Overrides do (instanceOverride), and the domain cannot be overridden (MATERIAL_INSTANCE). An unknown value is INVALID_ENUM, listing the valid ones, and leaves the asset as it was; a material function is UNSUPPORTED_ASSET_TYPE; engine content is refused (ENGINE_CONTENT), so duplicate it into /Game first.',
    topics: ['blend mode', 'material domain', 'shading model', 'two sided', 'make material transparent', 'unlit material'],
    members: { blend_mode: 'set_blend_mode', domain: 'set_material_domain', shading_model: 'set_shading_model', two_sided: 'set_two_sided' },
  },
  {
    primary: 'set_material_parameter', selector: 'parameterKind',
    summary: 'Set material parameter values, chosen by parameterKind. On a material instance it overrides a parameter its parent publishes (a name it does not publish is PARAMETER_NOT_FOUND, listing the published ones); on a base material it sets the parameter node\'s default, refused while the material is open in its editor (ASSET_EDITOR_OPEN; the editor would write its own copy back on close). scalar takes a number value, vector value {r, g, b, a} or [r, g, b, a], texture texturePath, static_switch value true or false, and a missing value is refused; parameter takes parameterType (scalar, the default, vector or texture) with value, several at once with parameters, or several instances under one consent with assets. The asset is saved unless save is false (saved).',
    topics: ['material parameter', 'scalar parameter value', 'vector parameter value', 'texture parameter', 'static switch', 'change material color', 'set material color', 'set roughness', 'set metallic', 'set base color', 'override material parameter', 'set parameter on material instance'],
    members: { parameter: 'set_material_parameter', scalar: 'set_scalar_parameter_value', vector: 'set_vector_parameter_value', texture: 'set_texture_parameter_value', static_switch: 'set_static_switch_parameter_value' },
  },
  {
    primary: 'get_material_info', selector: 'info',
    summary: 'Inspect a material or material function: summary with its parameters and overrides, node lookup, node details/properties/connections, node chains and connected subgraphs.',
    topics: ['material info', 'get material parameters', 'material parameters', 'material node', 'node connections', 'find node', 'material function info', 'inspect material graph', 'read material graph', 'material graph nodes', 'material node details', 'list material nodes', 'nodes in a material'],
    members: {
      material: 'get_material_info', function: 'get_material_function_info', find_node: 'find_node', node_details: 'get_material_node_details',
      node_properties: 'get_node_properties', node_connections: 'get_node_connections', node_chain: 'get_node_chain', subgraph: 'get_connected_subgraph',
    },
  },
  // struct
  {
    primary: 'edit_struct', selector: 'edit',
    summary: 'Create a user-defined struct or edit it: members, defaults, metadata, types, order, rename, duplicate, import, recompile, refresh dependencies, instanced struct properties.',
    topics: ['user defined struct', 'struct member', 'struct default', 'recompile struct', 'instanced struct', 'make struct', 'new struct', 'define struct'],
    members: byName(['create_struct', 'add_struct_member', 'rename_struct_member', 'reorder_struct_members', 'set_struct_member_default',
      'set_struct_member_metadata', 'set_struct_member_type', 'rename_struct', 'duplicate_struct', 'import_struct', 'recompile_struct',
      'refresh_struct_dependencies', 'set_instanced_struct_property']),
  },
  {
    primary: 'get_struct', selector: 'info',
    summary: 'Read structs, chosen by info. struct (read, members and export give the same reply) reads a Blueprint or native struct: name, status, validity and every member { guid, name, type, default, tooltip, containerType, metaData }, the members array import_struct takes back; list lists the Blueprint struct assets under path (default /Game/Structs, recursive); usage lists the assets that reference a Blueprint struct (assetPath, className, referencerType Blueprint, Struct, DataAsset or Other), narrowed by a searchScope path prefix; compare diffs two Blueprint structs member by member, paired by member guid so a rename reads as renamedFrom and renamedTo, each diff added, removed or changed with its type and default in each, plus tooltip and metadata mismatches (metadata from UE 5.5); instanced_property reads an FInstancedStruct property of an asset. Nothing is written.',
    topics: ['struct members', 'list structs', 'compare structs', 'export struct', 'struct usage', 'look at struct'],
    members: {
      struct: 'get_struct', read: 'read_struct', members: 'list_struct_members', list: 'list_structs', usage: 'search_struct_usage',
      compare: 'compare_structs', export: 'export_struct', instanced_property: 'get_instanced_struct_property',
    },
  },
  {
    primary: 'delete_struct', selector: 'deleteScope',
    summary: 'Delete a Blueprint Struct asset (deleteScope struct, the default: refused with STRUCT_IN_USE, naming referencers, while other assets use it, unless force deletes it anyway and recompiles the loaded Blueprints that used it; NOT_FOUND when there is none) or remove one member (member: by memberName or varGuid; the assets using the struct are refreshed, and it is saved unless save is false).',
    members: { struct: 'delete_struct', member: 'remove_struct_member' },
  },
  // texture
  {
    primary: 'create_texture', selector: 'kind',
    summary: 'Create a texture: gradient, noise, pattern, text, normal from height, ambient occlusion baked from a mesh, resized copy, channel pack/extract, or a combination of two textures.',
    topics: ['create texture', 'noise texture', 'gradient texture', 'text texture', 'text to texture', 'normal map', 'bake ambient occlusion', 'channel pack', 'resize texture', 'combine textures'],
    members: {
      gradient: 'create_gradient_texture', noise: 'create_noise_texture', pattern: 'create_pattern_texture', text: 'create_text_texture', ao_from_mesh: 'create_ao_from_mesh',
      normal_from_height: 'create_normal_from_height', resized: 'resize_texture', channel_pack: 'channel_pack', channel_extract: 'channel_extract', combined: 'combine_textures',
    },
  },
  {
    primary: 'adjust_texture', selector: 'adjust',
    summary: 'Edit the pixels of an 8-bit texture in place (assetPath; a 16-bit or HDR source is refused, and a grayscale one becomes BGRA8), chosen by adjust: curves maps the channel through curvePoints ({x, y} pairs from 0 to 1, at least two); levels remaps inBlack to inWhite (0 to 1) with gamma; blur averages over radius (default 2, 1 to 10); sharpen at strength (default 1, up to 5); desaturate by amount (default 1, fully gray); invert flips the colors. channel picks R, G, B or A for curves, levels and invert (otherwise the three color channels). The texture is saved unless save is false (saved), with no undo, so duplicate it first to keep the original; engine content is refused (ENGINE_CONTENT).',
    topics: ['adjust texture', 'blur texture', 'sharpen', 'desaturate', 'invert texture', 'levels', 'curves', 'grayscale texture', 'sharpen image'],
    members: { curves: 'adjust_curves', levels: 'adjust_levels', blur: 'blur', sharpen: 'sharpen', desaturate: 'desaturate', invert: 'invert' },
  },
  {
    primary: 'configure_texture', selector: 'setting',
    summary: 'Change one setting of a 2D texture asset (assetPath), chosen by setting; the texture is saved unless save is false (saved). compression sets compressionSettings by engine name, with or without the TC_ prefix (Normalmap, Masks, Grayscale, HDR, EditorIcon for UI, BC7 ...); texture_group sets textureGroup, with or without TEXTUREGROUP_ (World, Character, UI, Effects ...), so the texture follows that group\'s LOD and streaming rules; an unknown name in either is refused with the valid names. lod_bias drops lodBias top mips at runtime (1 halves each side, 0 restores full size). virtual_texture sets virtualTextureStreaming (required); materials sampling the texture then need the matching sampler type (Virtual Color and so on, or back) or they stop compiling. streaming_priority sets neverStream (required): true keeps every mip resident; there is no per-texture streaming priority.',
    topics: ['texture compression', 'lod bias', 'streaming priority', 'texture group', 'virtual texture', 'reduce texture size'],
    members: { compression: 'set_compression_settings', lod_bias: 'set_lod_bias', streaming_priority: 'set_streaming_priority', texture_group: 'set_texture_group', virtual_texture: 'configure_virtual_texture' },
  },
];
