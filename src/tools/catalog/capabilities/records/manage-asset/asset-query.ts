// Asset query/analysis records: dependency graphs, source control state,
// metadata, tags, validation, redirectors, thumbnails, reports, and the
// analyze_graph/get_asset_graph transport divergence.

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
  graphType: str('Graph kind analyzed: Material, Blueprint, or None.'),
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
  message: str('Explanation when the asset type carries no graph.'),
}, ['success']);

export const ASSET_QUERY_RECORDS: readonly RecordSpec[] = [
  r('get_dependencies', 'asset', 'List the packages an asset uses, or with referencers the packages that use it. Direct packages only; lookup "graph" walks dependencies recursively.',
    schema({
      assetPath: ASSET_PATH,
      referencers: bool('List the packages that USE this asset (Blueprints that spawn it, levels that place it) instead of the ones it uses; check this before deleting or replacing an asset.'),
    }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('List the Blueprints and levels that use a Niagara system', { assetPath: '/Game/FX/NS_Puff', referencers: true }, { success: true })] }
  ),

  r('get_source_control_state', 'asset', 'Retrieve source-control state for an asset.',
    schema({ assetPath: ASSET_PATH, assetPaths: arr('Several asset paths to query in one call.'), recursive: bool('Also report every /Game package the assets depend on, transitively (up to 512).') }, [], ['assetPath', 'assetPaths']),
    OK, READ, READ_POLICY, LOW,
    { examples: [ex('Check whether a mesh is checked out', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),

  r('analyze_graph', 'asset', 'Analyze the node graph inside a material or Blueprint asset.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    ANALYZE_GRAPH_OK, READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'get_asset_graph',
      examples: [ex('Inspect a material\'s expression graph', { assetPath: '/Game/Materials/M_Base' },
        { success: true, graphType: 'Material', nodeCount: 4, parameterCount: 2, blendMode: 'BLEND_Opaque' })] }
  ),

  r('get_asset_graph', 'asset', 'Retrieve the asset reference graph directly via the get_asset_graph bridge action.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth (clamped to 8).') }, ['assetPath']),
    GRAPH_OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('Read the reference graph through the direct bridge route', { assetPath: '/Game/Materials/M_Base', maxDepth: 2 }, { success: true })] }
  ),

  r('create_thumbnail', 'asset', 'Generate a thumbnail for an asset.',
    schema({ assetPath: ASSET_PATH, width: num('Thumbnail width.'), height: num('Thumbnail height.'), outputPath: str('Project-relative PNG file to write, e.g. Saved/Thumbnails/SM_Crate.png.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'generate_thumbnail',
      examples: [ex('Render a 256x256 thumbnail', { assetPath: '/Game/Meshes/SM_Crate', width: 256, height: 256 }, { success: true })] }
  ),

  r('set_tags', 'asset', 'Set tags on an asset.',
    schema({ assetPath: ASSET_PATH, tags: arr('Tags to set.') }, ['assetPath', 'tags']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_tags',
      examples: [ex('Tag an asset for review', { assetPath: '/Game/Meshes/SM_Crate', tags: ['Reviewed', 'Prop'] }, { success: true })] }
  ),

  r('get_metadata', 'asset', 'Retrieve metadata and tags for an asset.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    schema({ success: bool('Operation succeeded.'), assetPath: ASSET_PATH, tags: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Asset Registry tags (key-value).' }, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Custom package metadata (key-value).' } }, ['success']),
    READ, READ_POLICY, LOW,
    { examples: [ex('Read metadata for a mesh', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true, assetPath: '/Game/Meshes/SM_Crate', metadata: { Author: 'ArtTeam' } })] }
  ),

  r('set_metadata', 'asset', 'Set metadata key-value pairs on an asset.',
    schema({ assetPath: ASSET_PATH, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key-value pairs.' } }, ['assetPath', 'metadata']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_metadata',
      examples: [ex('Record authoring provenance', { assetPath: '/Game/Meshes/SM_Crate', metadata: { Author: 'ArtTeam', Revision: '3' } }, { success: true })] }
  ),

  r('validate', 'asset', 'Validate an asset for errors.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('Validate a material before submit', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),

  r('fixup_redirectors', 'asset', 'Fix up redirector assets in a directory.',
    schema({ directoryPath: str('Directory path to fix up.'), path: str('Alternative directory path.'), checkoutFiles: bool('Check the referencing packages out of source control before resaving them.') }, [], ['directoryPath', 'path']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'fixup_redirectors',
      examples: [ex('Clean up redirectors left by a move', { directoryPath: '/Game/Meshes' }, { success: true })] }
  ),

  r('refresh_blueprints', 'asset', 'Refresh every node of the Blueprints under a folder (or listed), then compile and save each; run it after renaming or moving classes so casts and node titles follow.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths.') }, [], ['assetPaths', 'folderPath']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'refresh_blueprints',
      examples: [ex('Refresh the Blueprints after a class rename', { folderPath: '/Game/Blueprints' }, { success: true })] }
  ),

  r('find_by_tag', 'asset', 'Find /Game assets whose asset-registry tag matches (optionally a value).',
    schema({ tag: str('Tag name to search for.'), value: str('Optional tag value.') }, ['tag']),
    OK, READ, READ_POLICY, LOW,
    { examples: [ex('Find every asset tagged Reviewed', { tag: 'Reviewed' }, { success: true })] }
  ),

  r('generate_report', 'asset', 'Generate an asset report for a directory.',
    schema({ directory: str('Directory to report on.'), reportType: str('Report type.'), outputPath: str('Output file path.') }, []),
    OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('Report on the Meshes directory', { directory: '/Game/Meshes', reportType: 'summary', outputPath: '/Game/Reports/Meshes' }, { success: true })] }
  )
];
