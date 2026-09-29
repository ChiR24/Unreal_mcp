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
      whenToUse: [
        'A scene capture, or a Blueprint that draws to a texture, needs a render target asset to render into.',
        'A render target of a chosen size and pixel format must exist at a /Game path; one already there is reported, not replaced.',
      ],
      whenNotToUse: [
        'A capture actor must be pointed at a target or re-captured (use build_environment.configure_scene_capture).',
        'A texture with generated pixels such as a gradient or noise is wanted (use texture.create_texture).',
      ],
      examples: [ex('Create a 1024x1024 HDR render target', { name: 'RT_SceneCapture', packagePath: '/Game/RenderTargets', width: 1024, height: 1024, format: 'RTF_RGBA16f', save: true }, { success: true })] }
  ),
  r('generate_lods', 'asset', 'Generate LOD levels for a static mesh asset.',
    schema({ assetPath: ASSET_PATH, assetPaths: arr('Several asset paths to process in one call.'), lodCount: num('Number of LOD levels to generate (1-50, default 4).') }, [], ['assetPath', 'assetPaths']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { whenToUse: ['One or several static meshes need LOD levels generated so distant copies render cheaper.'],
      whenNotToUse: [
        'LOD reduction settings or screen sizes must be tuned by hand (use manage_geometry.configure_mesh_lods).',
        'The mesh should use Nanite instead of discrete LODs (use asset.nanite_rebuild_mesh).',
      ],
      examples: [ex('Generate four LODs for a prop', { assetPath: '/Game/Meshes/SM_Crate', lodCount: 4 }, { success: true })] }
  ),
  r('add_material_parameter', 'asset', 'Add a parameter to a material.',
    schema({ assetPath: str('Material asset path.'), parameterName: str('Parameter name.'), parameterType: str('Parameter type.'), value: { description: 'Parameter value.' } }, ['assetPath', 'parameterName']),
    OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A new scalar, vector, texture or static-switch parameter must be introduced on a material or material function.'],
      whenNotToUse: [
        'An existing parameter needs a new value on a material instance (use material.set_material_parameter).',
        'Another kind of node must be added to the material graph (use material.add_material_node).',
      ],
      examples: [ex('Add a scalar roughness parameter', { assetPath: '/Game/Materials/M_Base', parameterName: 'Roughness', parameterType: 'Scalar', value: 0.4 }, { success: true })] }
  ),
  r('list_instances', 'asset', 'List material instances using a parent material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { examples: [ex('List instances of a parent material', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),
  r('reset_instance_parameters', 'asset', 'Reset all parameter overrides on a material instance.',
    schema({ assetPath: str('Material instance asset path.') }, ['assetPath']),
    OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A material instance must return to its parent\'s values by dropping every parameter override.'],
      whenNotToUse: [
        'Only some overrides need new values (use material.set_material_parameter).',
        'A fresh instance of the parent is wanted instead (use material.create_material_instance).',
      ],
      examples: [ex('Drop every override on an instance', { assetPath: '/Game/Materials/MI_Base_Rusty' }, { success: true })] }
  ),
  r('exists', 'asset', 'Check whether an asset exists at a given path, or which of several do.',
    schema({ assetPath: ASSET_PATH, assetPaths: arr('Several asset paths checked in one call; the reply maps each to true or false under existsByPath.') }, [], ['assetPath', 'assetPaths']),
    OK, READ, READ_POLICY, LOW,
    { topics: ['asset exists', 'does asset exist', 'check asset', 'path exists'], dispatchAction: 'exists',
      whenToUse: ['A path must be tested for an asset, or several paths at once, before creating, importing or referencing one.'],
      whenNotToUse: ['A folder\'s contents must be browsed rather than one path tested (use asset.list).'],
      examples: [ex('Probe for an asset before creating it', { assetPath: '/Game/Meshes/SM_Crate' }, { success: true })] }
  ),
  r('get_material_stats', 'asset', 'Retrieve rendering statistics for a material.',
    schema({ assetPath: str('Material asset path.') }, ['assetPath']),
    OK, READ, READ_POLICY, LOW,
    { whenToUse: ['A material\'s shading model, blend mode, domain, node and parameter counts, or the input pins its Main result node accepts, must be read.'],
      whenNotToUse: ['The individual nodes of a material and their connections are needed (use material.get_material_info).'],
      examples: [ex('Read instruction counts for a material', { assetPath: '/Game/Materials/M_Base' }, { success: true })] }
  ),
  r('nanite_rebuild_mesh', 'asset', 'Rebuild a Nanite mesh representation.',
    schema({ assetPath: str('Static mesh asset path.'), trianglePercent: num('Percent of the source triangles Nanite keeps, 0-100 (default 100).') }, ['assetPath']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'manage_render',
      whenToUse: ['A static mesh needs Nanite enabled, or the percentage of triangles Nanite keeps changed.'],
      whenNotToUse: [
        'A dynamic mesh must be baked into a new Nanite static mesh asset (use manage_geometry.convert_to_nanite).',
        'Plain LOD levels are wanted for a mesh that will not use Nanite (use asset.process_asset with process=lods).',
      ],
      examples: [ex('Rebuild Nanite data after a mesh edit', { assetPath: '/Game/Meshes/SM_Rock' }, { success: true })] }
  ),
  r('bulk_rename', 'asset', 'Rename multiple assets in one call: by a pattern (search/replace, prefix, suffix) over a folder or a list, or to explicit new names with renames.',
    schema({
      folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths.'), searchText: str('Search pattern.'), pattern: str('Search pattern (used when searchText is absent).'), replaceText: str('Replacement text.'), replacement: str('Replacement text (used when replaceText is absent).'), prefix: str('Name prefix.'), suffix: str('Name suffix.'), checkoutFiles: bool('Check out files in source control.'),
      renames: {
        type: 'array',
        description: 'Explicit renames, each asset to its own new name in its own folder, all in this one call: [{sourcePath, newName}]. An entry whose asset is missing, or whose newName is taken, is skipped and named under skipped. Replaces the pattern fields.',
        items: { type: 'object', properties: { sourcePath: str('Asset to rename.'), newName: str('New asset name (no folder).') }, required: ['sourcePath', 'newName'], additionalProperties: false },
      },
    }, [], ['assetPaths', 'folderPath', 'renames']),
    OK, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'bulk_rename',
      whenToUse: [
        'A naming convention must be enforced across a folder or a list, such as swapping a prefix or adding a suffix to every asset.',
        'Several unrelated assets must each get a specific new name in one call (the renames list).',
      ],
      whenNotToUse: ['One asset or one folder gets a new name (use asset.rename).'],
      examples: [ex('Re-prefix every mesh in a folder', { folderPath: '/Game/Meshes', searchText: 'Mesh_', replaceText: 'SM_', checkoutFiles: true }, { success: true })] }
  ),
  r('bulk_delete', 'asset', 'Delete multiple assets by folder or explicit paths.',
    schema({ folderPath: str('Folder path for bulk operation.'), assetPaths: arr('Explicit asset paths to delete.'), fixupRedirectors: bool('Default true. After a delete that removed something, resolve the redirectors that already exist in the folders the assets were deleted from, subfolders included: their referencers are re-pointed and re-saved, then the redirectors nothing points at any more are removed. Deleting creates no redirectors, so this only cleans older ones.') }, [], ['assetPaths', 'folderPath']),
    OK, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH,
    { dispatchAction: 'bulk_delete',
      whenToUse: [
        'Every asset under a folder, or a long list of assets, must be deleted in one call.',
        'Old redirectors in the folders the assets are deleted from must be resolved in the same call (on by default).',
      ],
      whenNotToUse: [
        'Assets that other assets still use must be kept, with the blocked ones reported (use asset.delete).',
        'The folder itself must be removed as well as its assets (use asset.delete with the folder path).',
      ],
      examples: [ex('Delete two obsolete assets and clean redirectors', { assetPaths: ['/Game/MCPTest/OldA', '/Game/MCPTest/OldB'], fixupRedirectors: true }, { success: true })] }
  ),
  r('source_control_checkout', 'asset', 'Check out assets in source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to check out.') }, []),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'source_control_checkout',
      whenToUse: ['Assets must be checked out of revision control before they are edited.'],
      whenNotToUse: ['The checked-out or modified state of assets is wanted (use asset.query_asset for source control state).'],
      examples: [ex('Check out two materials before editing', { paths: ['/Game/Materials/M_Base', '/Game/Materials/M_Trim'] }, { success: true })] }
  ),
  r('source_control_submit', 'asset', 'Submit checked-out assets to source control.',
    schema({ assetPath: ASSET_PATH, paths: arr('Asset paths to submit.'), description: str('Submit description.') }, ['description']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'source_control_submit',
      whenToUse: ['Checked-out assets must be submitted to the revision-control server with a change description.'],
      whenNotToUse: [
        'Unsaved editor changes must reach disk first (use control_editor.save_all).',
        'Every change in the project is wanted as one Git snapshot, without listing assets (use sourceControlOp=commit_all).',
      ],
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
      whenToUse: ['A revision-control provider such as Git or Perforce must be selected before checkout or submit can work.'],
      whenNotToUse: ['Only the state of particular assets is wanted (use asset.query_asset for source control state).'],
      examples: [ex('Point the editor at the Git provider', { provider: 'Git' }, { success: true, provider: 'Git' })] }
  ),
  r('source_control_init', 'asset', 'Create a repository for this project, write an Unreal .gitignore, make the first commit and select the Git provider.',
    schema({ description: str('First commit message. Defaults to "Initial commit".'), userName: str('Commit author name, written to the repository config only.'), userEmail: str('Commit author email, written to the repository config only.') }, []),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'source_control_init',
      whenToUse: ['A project with no repository needs one, with an Unreal .gitignore and a first commit, in one call.'],
      whenNotToUse: ['A Perforce or Subversion provider is wanted; init builds only a Git repository (select the provider with sourceControlOp=enable).'],
      examples: [ex('Put a fresh project under revision control', { description: 'Initial commit', userName: 'Dev', userEmail: 'dev@example.com' }, { success: true, committed: true })] }
  ),
  r('source_control_commit_all', 'asset', 'Stage every change in the project and commit it as a snapshot.',
    schema({ description: str('Commit message.'), userName: str('Commit author name, written to the repository config only.'), userEmail: str('Commit author email, written to the repository config only.') }, ['description']),
    OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchAction: 'source_control_commit_all',
      whenToUse: ['A snapshot of every change in the project is wanted as one commit, for example after a batch of edits.'],
      whenNotToUse: [
        'Unsaved editor changes must reach disk first (use control_editor.save_all).',
        'Only chosen assets must go in, or the project uses Perforce or Subversion (use sourceControlOp=submit).',
      ],
      examples: [ex('Snapshot the project after a batch of edits', { description: 'Roster pass: 12 distinct brawler kits' }, { success: true, committed: true })] }
  )
];
