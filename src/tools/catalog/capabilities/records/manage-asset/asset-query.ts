// Asset query/analysis records: dependency graphs, source control state,
// metadata, tags, validation, redirectors, thumbnails, reports, and the
// analyze_graph/get_asset_graph transport divergence.

import type { RecordSpec } from './builder.js';
import { arr, bool, divergence, ex, LOW, MEDIUM, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ASSET_QUERY_RECORDS: readonly RecordSpec[] = [
  r('get_dependencies', 'asset', 'Retrieve the dependency graph for an asset.',
    schema({ assetPath: ASSET_PATH, recursive: bool('Recurse into dependencies.'), maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool',
      examples: [ex('Resolve a material\'s dependencies three levels deep', { assetPath: '/Game/Materials/M_Base', recursive: true, maxDepth: 3 }, { success: true })] }
  ),

  r('get_source_control_state', 'asset', 'Retrieve source-control state for an asset.',
    schema({ assetPath: ASSET_PATH, recursive: bool('Recurse into dependencies.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { dispatchAction: 'asset_query', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes get_source_control_state through the asset_query bridge action, not manage_asset.'),
      examples: [ex('Check whether a mesh is checked out', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),

  r('analyze_graph', 'asset', 'Analyze the asset reference graph starting from an asset path.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'get_asset_graph', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes analyze_graph to the get_asset_graph bridge action. analyze_graph and get_asset_graph are distinct capabilities sharing a graph-analysis domain.'),
      examples: [ex('Analyze the reference graph around a material', { assetPath: '/Game/Materials/M_Base', maxDepth: 2 }, { success: true })] }
  ),

  r('get_asset_graph', 'asset', 'Retrieve the asset reference graph directly via the get_asset_graph bridge action.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool',
      normalization: divergence('Direct bridge dispatch via get_asset_graph subAction. Distinct from analyze_graph which routes here as a transport alias.'),
      examples: [ex('Read the reference graph through the direct bridge route', { assetPath: '/Game/Materials/M_Base', maxDepth: 2 }, { success: true })] }
  ),

  r('create_thumbnail', 'asset', 'Generate a thumbnail for an asset.',
    schema({ assetPath: ASSET_PATH, width: num('Thumbnail width.'), height: num('Thumbnail height.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'generate_thumbnail', dispatchMode: 'tool',
      normalization: divergence('Transport divergence: TS dispatches create_thumbnail with subAction generate_thumbnail.'),
      examples: [ex('Render a 256x256 thumbnail', { assetPath: '/Game/Meshes/SM_Crate', width: 256, height: 256 }, { success: true })] }
  ),

  r('set_tags', 'asset', 'Set tags on an asset.',
    schema({ assetPath: ASSET_PATH, tags: arr('Tags to set.') }, ['assetPath', 'tags']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_tags', dispatchMode: 'action',
      examples: [ex('Tag an asset for review', { assetPath: '/Game/Meshes/SM_Crate', tags: ['Reviewed', 'Prop'] }, { success: true })] }
  ),

  r('get_metadata', 'asset', 'Retrieve metadata and tags for an asset.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool',
      examples: [ex('Read metadata for a mesh', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),

  r('set_metadata', 'asset', 'Set metadata key-value pairs on an asset.',
    schema({ assetPath: ASSET_PATH, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key-value pairs.' } }, ['assetPath', 'metadata']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'set_metadata', dispatchMode: 'action',
      examples: [ex('Record authoring provenance', { assetPath: '/Game/Meshes/SM_Crate', metadata: { Author: 'ArtTeam', Revision: '3' } }, { success: true })] }
  ),

  r('validate', 'asset', 'Validate an asset for errors.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool',
      examples: [ex('Validate a material before submit', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),

  r('fixup_redirectors', 'asset', 'Fix up redirector assets in a directory.',
    schema({ directoryPath: str('Directory path to fix up.'), path: str('Alternative directory path.') }, [], ['directoryPath', 'path']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'fixup_redirectors', dispatchMode: 'action',
      examples: [ex('Clean up redirectors left by a move', { directoryPath: '/Game/Meshes' }, { success: true })] }
  ),

  r('find_by_tag', 'asset', 'Find assets by tag with bounded pagination.',
    schema({ tag: str('Tag name to search for.'), value: str('Optional tag value.') }, ['tag']),
    OK, READ, READ_POLICY, LOW,
    { dispatchAction: 'asset_query', dispatchMode: 'action',
      examples: [ex('Find every asset tagged Reviewed', { tag: 'Reviewed' }, { success: true })] }
  ),

  r('generate_report', 'asset', 'Generate an asset report for a directory.',
    schema({ directory: str('Directory to report on.'), reportType: str('Report type.'), outputPath: str('Output file path.') }, []),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool',
      examples: [ex('Report on the Meshes directory', { directory: '/Game/Meshes', reportType: 'summary', outputPath: '/Game/Reports/Meshes' }, { success: true })] }
  )
];
