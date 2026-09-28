// Asset advanced records: render targets, LODs, material parameters,
// instances, nanite, bulk operations, and source-control workflow.
// Models transport divergences for create_render_target (manage_texture)
// and nanite_rebuild_mesh (manage_render).

import type { RecordSpec } from './builder.js';
import { arr, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, num, r, READ, READ_POLICY, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ASSET_ADVANCED_RECORDS: readonly RecordSpec[] = [
  r('create_render_target', 'asset', 'Create a render target texture asset.',
    schema({ name: str('Render target name.'), packagePath: str('Package path (default /Game/Textures).'), renderTargetPath: str('Full asset path, e.g. /Game/RenderTargets/RT_Capture; replaces name and packagePath.'), width: num('Width in pixels.'), height: num('Height in pixels.'), format: str('Pixel format.'), save: bool('Save after creation. Defaults to true.') }, [], ['name', 'renderTargetPath']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'manage_texture',
      examples: [ex('Create a 1024x1024 HDR render target', { name: 'RT_SceneCapture', packagePath: '/Game/RenderTargets', width: 1024, height: 1024, format: 'RTF_RGBA16f', save: true }, { success: true })] }
  ),
  r('generate_lods', 'asset', 'Generate LOD levels for a static mesh asset.',
    schema({ assetPath: ASSET_PATH, assetPaths: arr('Several asset paths to process in one call.'), lodCount: num('Number of LOD levels to generate (1-50, default 4).') }, [], ['assetPath', 'assetPaths']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { examples: [ex('Generate four LODs for a prop', { assetPath: '/Game/Meshes/SM_Crate', lodCount: 4 }, { success: true })] }
  ),
  r('add_material_parameter', 'asset', 'Add a parameter to a material.',
    schema({ assetPath: str('Material asset path.'), parameterName: str('Parameter name.'), parameterType: str('Parameter type.'), value: { description: 'Parameter value.' } }, ['assetPath', 'parameterName']),
    OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Add a scalar roughness parameter', { assetPath: '/Game/Materials/M_Base', parameterName: 'Roughness', parameterType: 'Scalar', value: 0.4 }, { success: true })] }
  ),
  r('list_instances', 'asset', 'List material instances using a parent material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { examples: [ex('List instances of a parent material', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),
  r('reset_instance_parameters', 'asset', 'Reset all parameter overrides on a material instance.',
    schema({ assetPath: str('Material instance asset path.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Drop every override on an instance', { assetPath: '/Game/Materials/MI_Base_Rusty' }, { success: true })] }
  ),
  r('exists', 'asset', 'Check whether an asset exists at a given path.',
    schema({ assetPath: ASSET_PATH }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { topics: ['asset exists', 'does asset exist', 'check asset', 'path exists'], dispatchAction: 'exists',
      examples: [ex('Probe for an asset before creating it', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),
  r('get_material_stats', 'asset', 'Retrieve rendering statistics for a material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read instruction counts for a material', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),
  r('nanite_rebuild_mesh', 'asset', 'Rebuild a Nanite mesh representation.',
    schema({ assetPath: str('Static mesh asset path.'), trianglePercent: num('Percent of the source triangles Nanite keeps, 0-100 (default 100).') }, ['assetPath']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'manage_render',
      examples: [ex('Rebuild Nanite data after a mesh edit', { assetPath: '/Game/Meshes/SM_Rock' }, { success: true })] }
  ),
  r('bulk_rename', 'asset', 'Rename multiple assets by pattern or explicit paths.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths.'), searchText: str('Search pattern.'), pattern: str('Search pattern (used when searchText is absent).'), replaceText: str('Replacement text.'), replacement: str('Replacement text (used when replaceText is absent).'), prefix: str('Name prefix.'), suffix: str('Name suffix.'), checkoutFiles: bool('Check out files in source control.') }, [], ['assetPaths', 'folderPath']),
    OK, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'bulk_rename',
      examples: [ex('Re-prefix every mesh in a folder', { folderPath: '/Game/Meshes', searchText: 'Mesh_', replaceText: 'SM_', checkoutFiles: true }, { success: true })] }
  ),
  r('bulk_delete', 'asset', 'Delete multiple assets by folder or explicit paths.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths to delete.'), showConfirmation: bool('Show confirmation prompt.'), fixupRedirectors: bool('Fix up redirectors left behind by the deletion.') }, [], ['assetPaths', 'folderPath']),
    OK, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH,
    { dispatchAction: 'bulk_delete',
      examples: [ex('Delete two obsolete assets and clean redirectors', { assetPaths: ['/Game/MCPTest/OldA', '/Game/MCPTest/OldB'], showConfirmation: false, fixupRedirectors: true }, { success: true })] }
  ),
  r('source_control_checkout', 'asset', 'Check out assets in source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to check out.') }, []),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'source_control_checkout',
      examples: [ex('Check out two materials before editing', { paths: ['/Game/Materials/M_Base', '/Game/Materials/M_Trim'] }, { success: true })] }
  ),
  r('source_control_submit', 'asset', 'Submit checked-out assets to source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to submit.'), description: str('Submit description.') }, ['description']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'source_control_submit',
      examples: [ex('Submit the edited materials', { paths: ['/Game/Materials/M_Base'], description: 'Retune base material roughness' }, { success: true })] }
  ),
  // The plugin has dispatched source_control_enable since the source-control
  // handlers were written, but no record ever published it, so the only way to
  // put a project under revision control was the editor's own login dialog --
  // the one thing an automation caller cannot reach.
  r('source_control_enable', 'asset', 'Enable revision control and select the provider (Git, Perforce, Subversion...).',
    schema({ provider: str('Provider name as the editor registers it, e.g. Git or Perforce. Omit to report the current provider without changing it.') }, []),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'source_control_enable',
      examples: [ex('Point the editor at the Git provider', { provider: 'Git' }, { success: true, provider: 'Git' })] }
  ),
  r('source_control_init', 'asset', 'Create a repository for this project, write an Unreal .gitignore, make the first commit and select the Git provider.',
    schema({ description: str('First commit message. Defaults to "Initial commit".'), userName: str('Commit author name, written to the repository config only.'), userEmail: str('Commit author email, written to the repository config only.') }, []),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'source_control_init',
      examples: [ex('Put a fresh project under revision control', { description: 'Initial commit', userName: 'Dev', userEmail: 'dev@example.com' }, { success: true, committed: true })] }
  ),
  r('source_control_commit_all', 'asset', 'Stage every change in the project and commit it as a snapshot.',
    schema({ description: str('Commit message.'), userName: str('Commit author name, written to the repository config only.'), userEmail: str('Commit author email, written to the repository config only.') }, ['description']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'source_control_commit_all',
      examples: [ex('Snapshot the project after a batch of edits', { description: 'Roster pass: 12 distinct brawler kits' }, { success: true, committed: true })] }
  )
];
