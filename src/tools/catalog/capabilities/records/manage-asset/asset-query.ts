// Asset query/analysis records: dependency graphs, source control state,
// metadata, tags, validation, redirectors, thumbnails, reports, and the two
// graph capabilities (analyze_graph reads the nodes inside one asset,
// get_asset_graph walks package references).

import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, ex, LOW, MEDIUM, num, READ, READ_POLICY, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

// The graph walk is bounded (depth clamped, total nodes capped) so it always
// returns instead of running unbounded on the game thread — previously it could
// produce NO response at all until the transport timed out after 300 s. These
// fields carry the graph and say plainly when the result was cut short; without
// declaring them, output projection would drop the graph itself.
const GRAPH_OK = schema({
  success: bool('Operation succeeded.'),
  graph: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Adjacency map of asset path -> array of dependency paths.' },
  nodeCount: num('Number of distinct assets visited.'),
  maxDepth: num('Traversal depth actually used after clamping.'),
  truncated: bool('True when the walk hit the depth or node ceiling and the graph is partial.'),
}, ['success']);

// analyze_graph does NOT share GRAPH_OK. Despite the adjacent name it runs a
// different handler (…/Analysis/…GraphReport.cpp) that inspects the node graph
// INSIDE one material or Blueprint, and emits none of GRAPH_OK's fields but
// `nodeCount`. Under GRAPH_OK, output projection therefore discarded the whole
// analysis and left a bare success — the report the capability exists to give.
const ANALYZE_GRAPH_OK = schema({
  success: bool('Operation succeeded.'),
  assetPath: ASSET_PATH,
  assetClass: str('Concrete UClass name of the analyzed asset.'),
  graphType: str('Graph kind analyzed: Material, Blueprint, None, or the class of an asset whose graph another action reads (see nextCall).'),
  nodeCount: num('Material expression nodes in the graph.'),
  parameterCount: num('Material parameter expressions in the graph.'),
  textureSampleCount: num('Texture sample expressions in the graph.'),
  parameters: arr('Material parameter names.'),
  isMaterialInstance: bool('True when the asset is a material instance.'),
  isTwoSided: bool('True when the material renders two-sided.'),
  isMasked: bool('True when the material uses masked blending.'),
  blendMode: str('Material blend mode enum name.'),
  shadingModel: str('First matching shading model name.'),
  blueprintType: str('Blueprint kind: Class, Interface, MacroLibrary, or FunctionLibrary.'),
  totalNodes: num('Total nodes across every Blueprint graph.'),
  graphCount: num('Number of graphs in the Blueprint.'),
  graphs: arrObj('Per-graph breakdown (name, nodeCount).'),
  message: str('Explanation when the asset type carries no graph, or which action reads it.'),
  nextCall: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'A MetaSound, Niagara or Behavior Tree asset: the execute call of the action that reads its graph.' },
}, ['success']);

export const ASSET_QUERY_RECORDS: readonly RecordSpec[] = [
  r('get_dependencies', 'asset', 'List the packages an asset uses, or with referencers the packages that use it. Direct packages only; lookup "graph" walks dependencies recursively.',
    schema({
      assetPath: ASSET_PATH,
      referencers: bool('List the packages that USE this asset (Blueprints that spawn it, levels that place it) instead of the ones it uses; check this before deleting or replacing an asset.'),
    }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { whenToUse: [
        'Before deleting or replacing an asset, the packages, Blueprints and levels that use it must be listed.',
        'The packages an asset itself uses must be listed (direct ones only).',
      ],
      whenNotToUse: ['The node graph inside a material or Blueprint is wanted, not package links (use asset.query_asset with lookup=graph).'],
      examples: [ex('List the Blueprints and levels that use a Niagara system', { assetPath: '/Game/FX/NS_Puff', referencers: true }, { success: true })] }
  ),

  r('get_source_control_state', 'asset', 'Retrieve source-control state for an asset.',
    schema({ assetPath: ASSET_PATH, assetPaths: arr('Several asset paths to query in one call.'), recursive: bool('Also report every /Game package the assets depend on, transitively (up to 512).') }, [], ['assetPath', 'assetPaths']),
    OK, READ, READ_POLICY, LOW,
    { whenToUse: ['Before editing, it must be known whether assets are checked out, modified or held by someone else.'],
      whenNotToUse: ['Assets must be checked out or submitted (use asset.source_control).'],
      examples: [ex('Check whether a mesh is checked out', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),

  r('analyze_graph', 'asset', 'Analyze the node graph inside a material or Blueprint asset; a MetaSound, Niagara or Behavior Tree asset answers with the nextCall that reads its graph.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    ANALYZE_GRAPH_OK, READ, READ_POLICY, MEDIUM,
    { whenToUse: ['A material or Blueprint must be summarised: node counts, parameters, blend mode or graph count.'],
      whenNotToUse: ['One Blueprint\'s variables, functions or components are wanted (use blueprint.get_blueprint).'],
      examples: [ex('Inspect a material\'s expression graph', { assetPath: '/Game/Materials/M_Base' },
        { success: true, graphType: 'Material', nodeCount: 4, parameterCount: 2, blendMode: 'BLEND_Opaque' })] }
  ),

  r('get_asset_graph', 'asset', 'Retrieve the asset reference graph directly via the get_asset_graph bridge action.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth (clamped to 8).') }, ['assetPath']),
    GRAPH_OK, READ, READ_POLICY, MEDIUM,
    { whenToUse: ['The reference graph of an asset must be walked several levels deep in one call (depth is capped at 8).'],
      whenNotToUse: [
        'Only the direct packages an asset uses, or the packages that use it, are needed (use lookup=dependencies, which is cheaper).',
        'A material\'s package references are wanted; a plain material answers here with its expression nodes (use lookup=dependencies).',
      ],
      examples: [ex('Read the reference graph through the direct bridge route', { assetPath: '/Game/Materials/M_Base', maxDepth: 2 }, { success: true })] }
  ),

  r('create_thumbnail', 'asset', 'Generate a thumbnail for an asset.',
    schema({ assetPath: ASSET_PATH, width: num('Thumbnail width.'), height: num('Thumbnail height.'), outputPath: str('Project-relative PNG file to write, e.g. Saved/Thumbnails/SM_Crate.png.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'generate_thumbnail',
      whenToUse: ['An asset needs a preview image, optionally written as a PNG file inside the project.'],
      whenNotToUse: ['The image should show the editor viewport or a level camera view (use control_editor.screenshot).'],
      examples: [ex('Render a 256x256 thumbnail', { assetPath: '/Game/Meshes/SM_Crate', width: 256, height: 256 }, { success: true })] }
  ),

  r('set_tags', 'asset', 'Set tags on an asset, stored as package metadata: each tag name is written with the value "true".',
    schema({ assetPath: ASSET_PATH, tags: arr('Tag names to set; each is written as package metadata with the value "true".') }, ['assetPath', 'tags']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_tags',
      whenToUse: ['Simple labels such as Reviewed or Prop must be attached to an asset as package metadata; the change stays unsaved until the asset is saved.'],
      whenNotToUse: [
        'Actors in a level need a tag (use control_actor.add_tag).',
        'Assets must be found by the tag afterwards. asset.query_asset lookup=by_tag reads the asset registry, which carries a package-metadata tag only when its name is listed under Project Settings > Asset Manager > Metadata Tags For Asset Registry and the asset has been saved; otherwise it finds nothing.',
        'The tags on an asset must be read back (use asset.inspect_asset lookup=metadata, which lists them under metadata at once).',
      ],
      examples: [ex('Tag an asset for review, as package metadata', { assetPath: '/Game/Meshes/SM_Crate', tags: ['Reviewed', 'Prop'] }, { success: true })] }
  ),

  r('get_metadata', 'asset', 'Retrieve metadata and tags for an asset.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    schema({ success: bool('Operation succeeded.'), assetPath: ASSET_PATH, tags: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Asset Registry tags (key-value). A tag written by set_tags appears here only when its name is listed under Project Settings > Asset Manager > Metadata Tags For Asset Registry.' }, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Package metadata (key-value): what set_metadata wrote, and each set_tags tag as its name with the value "true".' } }, ['success']),
    READ, READ_POLICY, LOW,
    { whenToUse: ['What was recorded on an asset must be read back: its registry tags and the package metadata that asset.set_metadata writes, tags included (a tag set with kind=tags shows under metadata as "true").'],
      whenNotToUse: ['The asset\'s own values are wanted, such as a texture size or material parameters (use texture.get_texture_info or material.get_material_info).'],
      examples: [ex('Read metadata for a mesh', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true, assetPath: '/Game/Meshes/SM_Crate', metadata: { Author: 'ArtTeam' } })] }
  ),

  r('set_metadata', 'asset', 'Set metadata key-value pairs on an asset.',
    schema({ assetPath: ASSET_PATH, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key-value pairs.' } }, ['assetPath', 'metadata']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_metadata',
      whenToUse: ['Key and value notes such as author or revision must be attached to an asset; the change stays unsaved until the asset is saved.'],
      whenNotToUse: [
        'The stored values must be read back (use asset.inspect_asset).',
        'A property of the asset itself must change (use inspect.set_property).',
      ],
      examples: [ex('Record authoring provenance', { assetPath: '/Game/Meshes/SM_Crate', metadata: { Author: 'ArtTeam', Revision: '3' } }, { success: true })] }
  ),

  r('validate', 'asset', 'Confirm an asset exists and loads. It runs no content, reference or data checks.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { whenToUse: ['An asset must be confirmed to exist and load before other calls rely on it (no content, reference or data checks are run).'],
      whenNotToUse: ['Every asset in a folder must be checked for load errors (use system_control.validate_assets).'],
      examples: [ex('Confirm a material exists and loads', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),

  r('fixup_redirectors', 'asset', 'Fix up redirector assets in a directory.',
    schema({ directoryPath: str('Directory path to fix up.'), path: str('Alternative directory path.'), checkoutFiles: bool('Check the referencing packages out of source control before resaving them.') }, [], ['directoryPath', 'path']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'fixup_redirectors',
      whenToUse: ['Redirectors left in a folder by earlier moves or renames must be resolved: referencers are repointed and the redirectors deleted.'],
      whenNotToUse: ['Redirectors should only be listed before anything changes (use asset.query_asset with lookup=search and classNames ObjectRedirector).'],
      examples: [ex('Clean up redirectors left by a move', { directoryPath: '/Game/Meshes' }, { success: true })] }
  ),

  r('refresh_blueprints', 'asset', 'Refresh every node of the Blueprints under a folder (or listed), then compile and save each; run it after renaming or moving classes so casts and node titles follow.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths.') }, [], ['assetPaths', 'folderPath']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'refresh_blueprints',
      whenToUse: ['Blueprints in a folder or a list need every node refreshed, then compiled and saved, for example after a class was renamed.'],
      whenNotToUse: ['One Blueprint only needs compiling (use blueprint.compile).'],
      examples: [ex('Refresh the Blueprints after a class rename', { folderPath: '/Game/Blueprints' }, { success: true })] }
  ),

  r('find_by_tag', 'asset', 'Find /Game assets that carry an asset-registry tag, optionally with a given value (compared ignoring case).',
    schema({ tag: str('Asset-registry tag name, for example ParentClass, which every Blueprint carries.'), value: str('Optional tag value to match, ignoring case.') }, ['tag']),
    OK, READ, READ_POLICY, LOW,
    { whenToUse: ['Assets under /Game must be found by an asset-registry tag, optionally with one value, such as the ParentClass tag every Blueprint carries. A tag written by asset.set_metadata with kind=tags is in the registry only when its name is listed under Project Settings > Asset Manager > Metadata Tags For Asset Registry and the asset has been saved; otherwise nothing matches (asset.inspect_asset lookup=metadata reads one asset).'],
      whenNotToUse: ['Actors placed in the open level must be found (use control_actor.find).'],
      examples: [ex('Find every Blueprint by its ParentClass registry tag', { tag: 'ParentClass' }, { success: true })] }
  ),

  r('generate_report', 'asset', 'Generate an asset report for a directory.',
    schema({ directory: str('Directory to report on.'), reportType: str('Report type.'), outputPath: str('Output file path.') }, []),
    OK, READ, READ_POLICY, MEDIUM,
    { whenToUse: ['Every asset under a directory, with name, path and class, must be listed in one reply, optionally saved as a JSON file in the project.'],
      whenNotToUse: ['A paged view of one folder is enough (use asset.list); the report returns the whole tree unpaged.'],
      examples: [ex('Report on the Meshes directory', { directory: '/Game/Meshes', reportType: 'summary', outputPath: '/Game/Reports/Meshes' }, { success: true })] }
  )
];
