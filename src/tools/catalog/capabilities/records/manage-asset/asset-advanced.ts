// Asset advanced records: render targets, LODs, material parameters,
// instances, nanite, bulk operations, and source-control workflow.
// Models transport divergences for create_render_target (manage_texture)
// and nanite_rebuild_mesh (manage_render).

import type { RecordSpec } from './builder.js';
import { arr, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, divergence, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ASSET_ADVANCED_RECORDS: readonly RecordSpec[] = [
  r('create_render_target', 'asset', 'Create a render target texture asset.',
    schema({ name: str('Render target name.'), packagePath: str('Package path (default /Game).'), width: num('Width in pixels.'), height: num('Height in pixels.'), format: str('Pixel format.'), save: bool('Save after creation.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'manage_texture', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes create_render_target through the manage_texture bridge action, not manage_asset.') }
  ),
  r('generate_lods', 'asset', 'Generate LOD levels for a static mesh asset.',
    schema({ assetPath: ASSET_PATH, lodCount: num('Number of LOD levels to generate.') }, ['assetPath', 'lodCount']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH, { dispatchMode: 'tool' }
  ),
  r('add_material_parameter', 'asset', 'Add a parameter to a material.',
    schema({ assetPath: str('Material asset path.'), parameterName: str('Parameter name.'), parameterType: str('Parameter type.'), value: { description: 'Parameter value.' } }, ['assetPath', 'parameterName']),
    OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'action' }
  ),
  r('list_instances', 'asset', 'List material instances using a parent material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW, { dispatchMode: 'action' }
  ),
  r('reset_instance_parameters', 'asset', 'Reset all parameter overrides on a material instance.',
    schema({ assetPath: str('Material instance asset path.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'action' }
  ),
  r('exists', 'asset', 'Check whether an asset exists at a given path.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, LOW, { dispatchAction: 'exists', dispatchMode: 'action' }
  ),
  r('get_material_stats', 'asset', 'Retrieve rendering statistics for a material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW, { dispatchMode: 'action' }
  ),
  r('nanite_rebuild_mesh', 'asset', 'Rebuild a Nanite mesh representation.',
    schema({ assetPath: str('Static mesh asset path.') }, ['assetPath']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'manage_render', dispatchMode: 'action',
      normalization: divergence('Transport divergence: TS routes nanite_rebuild_mesh through the manage_render bridge action, not manage_asset.') }
  ),
  r('bulk_rename', 'asset', 'Rename multiple assets by pattern or explicit paths.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths.'), searchText: str('Search pattern.'), pattern: str('Search pattern (used when searchText is absent).'), replaceText: str('Replacement text.'), replacement: str('Replacement text (used when replaceText is absent).'), prefix: str('Name prefix.'), suffix: str('Name suffix.'), checkoutFiles: bool('Check out files in source control.') }, []),
    OK, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM, { dispatchAction: 'bulk_rename', dispatchMode: 'action' }
  ),
  r('bulk_delete', 'asset', 'Delete multiple assets by folder or explicit paths.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths to delete.'), showConfirmation: bool('Show confirmation prompt.'), fixupRedirectors: bool('Fix up redirectors left behind by the deletion.') }, []),
    OK, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH, { dispatchAction: 'bulk_delete', dispatchMode: 'action' }
  ),
  r('source_control_checkout', 'asset', 'Check out assets in source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to check out.') }, []),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchAction: 'source_control_checkout', dispatchMode: 'action' }
  ),
  r('source_control_submit', 'asset', 'Submit checked-out assets to source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to submit.'), description: str('Submit description.') }, ['description']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchAction: 'source_control_submit', dispatchMode: 'action' }
  )
];
