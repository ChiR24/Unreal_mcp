// Asset query/analysis records: dependency graphs, source control state,
// metadata, tags, validation, redirectors, thumbnails, reports, and the
// analyze_graph/get_asset_graph transport divergence.

import type { RecordSpec } from './builder.js';
import { arr, bool, divergence, LOW, MEDIUM, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ASSET_QUERY_RECORDS: readonly RecordSpec[] = [
  r('get_dependencies', 'asset', 'Retrieve the dependency graph for an asset.',
    schema({ assetPath: ASSET_PATH, recursive: bool('Recurse into dependencies.'), maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),

  r('get_source_control_state', 'asset', 'Retrieve source-control state for an asset.',
    schema({ assetPath: ASSET_PATH, recursive: bool('Recurse into dependencies.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { dispatchAction: 'asset_query', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes get_source_control_state through the asset_query bridge action, not manage_asset.') }
  ),

  r('analyze_graph', 'asset', 'Analyze the asset reference graph starting from an asset path.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'get_asset_graph', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes analyze_graph to the get_asset_graph bridge action. analyze_graph and get_asset_graph are distinct capabilities sharing a graph-analysis domain.') }
  ),

  r('get_asset_graph', 'asset', 'Retrieve the asset reference graph directly via the get_asset_graph bridge action.',
    schema({ assetPath: ASSET_PATH, maxDepth: num('Maximum traversal depth.') }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool',
      normalization: divergence('Direct bridge dispatch via get_asset_graph subAction. Distinct from analyze_graph which routes here as a transport alias.') }
  ),

  r('create_thumbnail', 'asset', 'Generate a thumbnail for an asset.',
    schema({ assetPath: ASSET_PATH, width: num('Thumbnail width.'), height: num('Thumbnail height.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { dispatchAction: 'generate_thumbnail', dispatchMode: 'tool',
      normalization: divergence('Transport divergence: TS dispatches create_thumbnail with subAction generate_thumbnail.') }
  ),

  r('set_tags', 'asset', 'Set tags on an asset.',
    schema({ assetPath: ASSET_PATH, tags: arr('Tags to set.') }, ['assetPath', 'tags']),
    OK, WRITE, WRITE_POLICY, LOW, { dispatchAction: 'set_tags', dispatchMode: 'action' }
  ),

  r('get_metadata', 'asset', 'Retrieve metadata and tags for an asset.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }
  ),

  r('set_metadata', 'asset', 'Set metadata key-value pairs on an asset.',
    schema({ assetPath: ASSET_PATH, metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key-value pairs.' } }, ['assetPath', 'metadata']),
    OK, WRITE, WRITE_POLICY, LOW, { dispatchAction: 'set_metadata', dispatchMode: 'action' }
  ),

  r('validate', 'asset', 'Validate an asset for errors.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),

  r('fixup_redirectors', 'asset', 'Fix up redirector assets in a directory.',
    schema({ directoryPath: str('Directory path to fix up.'), path: str('Alternative directory path.') }, []),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchAction: 'fixup_redirectors', dispatchMode: 'action' }
  ),

  r('find_by_tag', 'asset', 'Find assets by tag with bounded pagination.',
    schema({ tag: str('Tag name to search for.'), value: str('Optional tag value.') }, ['tag']),
    OK, READ, READ_POLICY, LOW, { dispatchAction: 'asset_query', dispatchMode: 'action' }
  ),

  r('generate_report', 'asset', 'Generate an asset report for a directory.',
    schema({ directory: str('Directory to report on.'), reportType: str('Report type.'), outputPath: str('Output file path.') }, []),
    OK, READ, READ_POLICY, MEDIUM, { dispatchMode: 'tool' }
  )
];
