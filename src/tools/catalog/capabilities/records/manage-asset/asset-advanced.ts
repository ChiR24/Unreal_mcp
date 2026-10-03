// Asset advanced records: render targets, LODs, mesh material slots, material
// parameters, instances, nanite, bulk operations, and source-control workflow.
// Models transport divergences for create_render_target (manage_texture)
// and nanite_rebuild_mesh (manage_render).

import type { JsonObject } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, num, r, READ, READ_POLICY, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const ASSET_PATH = str('Canonical /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const MESH_MATERIAL_ENTRY: JsonObject = {
  type: 'object',
  properties: {
    slot: { type: ['integer', 'string'], description: 'The slot to set: its 0-based index (9) or its slot name ("hull"), as inspect_object objectKind=mesh lists them under materialSlots.' },
    materialPath: str('Material or material instance asset path, e.g. /Game/Materials/M_Hull. It must load: a path that does not is refused, never swapped for a default material.'),
  },
  required: ['slot', 'materialPath'],
  additionalProperties: false,
};

const MESH_MATERIALS_OUT = schema({
  success: bool('Operation succeeded.'),
  assetPath: ASSET_PATH,
  assetType: str('StaticMesh or SkeletalMesh.'),
  materialSlots: arrObj('Every slot of the mesh after the call, each {slotIndex, slotName, material (the asset path the slot holds, empty for none), changed (true when this call changed it)}.'),
  applied: num('Entries that were valid and applied.'),
  changed: num('Slots whose material this call changed; an entry that names the material a slot already holds changes nothing.'),
  refused: arrObj('Entries that were refused, each {index (its position in materials), slot (as given), materialPath, code, reason}: a slot index out of range, an unknown slot name, a material that does not load. A refused entry changes nothing, the call fails (MATERIAL_SLOTS_PARTIAL, or MATERIAL_SLOTS_REFUSED when no entry was valid) and the valid entries stay applied.'),
  saved: bool('Whether the mesh was saved.'),
  saveSkippedReason: str('Why nothing was saved: save was false, or the mesh is engine content, which is never written.'),
  details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' },
}, ['success']);

export const ASSET_ADVANCED_RECORDS: readonly RecordSpec[] = [
  r('create_render_target', 'asset', 'Create a render target texture asset.',
    schema({ name: str('Render target name.'), packagePath: str('Package path (default /Game/Textures).'), renderTargetPath: str('Full asset path, e.g. /Game/RenderTargets/RT_Capture; replaces name and packagePath.'), width: num('Width in pixels.'), height: num('Height in pixels.'), format: str('Pixel format.'), save: bool('Save after creation. Defaults to true.') }, [], ['name', 'renderTargetPath']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'manage_texture',
      topics: ['make render target'],
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
  r('set_mesh_materials', 'asset', 'Set the materials in the slots of a static or skeletal mesh asset, by slot index or slot name, so every placement of the mesh shows them.',
    schema({
      assetPath: ASSET_PATH,
      materials: {
        type: 'array', minItems: 1, items: MESH_MATERIAL_ENTRY,
        description: 'The slots to set, one entry per slot: [{slot, materialPath}]. slot is a 0-based index or a slot name (inspect_object objectKind=mesh lists both under materialSlots). Every entry is checked first. One that is refused (index out of range, unknown slot name, a material that does not load) is named under refused and fails the call, while the valid entries are applied together in a single rebuild of the mesh.',
      },
      save: bool('Save the mesh after the change. Defaults to true.'),
    }, ['assetPath', 'materials']),
    MESH_MATERIALS_OUT, WRITE, WRITE_POLICY, MEDIUM,
    { topics: ['set mesh materials', 'assign materials to a static mesh', 'mesh material slots', 'skeletal mesh materials', 'imported mesh grid material', 'materials on a mesh asset'],
      whenToUse: [
        'An imported mesh came in with every slot on the default grid material and each slot needs its own material, once for every placement of the mesh.',
        'Several slots of a static or skeletal mesh asset get their materials in one call, by index or by slot name.',
      ],
      whenNotToUse: [
        'Only one placed actor must look different from its mesh (use control_actor.set_material, which overrides a component and leaves the asset alone).',
        'It is not known yet which slot holds which part of the mesh (inspect.inspect_object with objectKind=mesh lists every slot with the triangle count and bounds of its geometry).',
      ],
      examples: [ex('Give an imported mesh its own materials, one slot by name and one by index', { assetPath: '/Game/Meshes/SM_Rider', materials: [{ slot: 'hull', materialPath: '/Game/Materials/M_Hull' }, { slot: 9, materialPath: '/Game/Materials/M_Claw' }] }, { success: true, assetPath: '/Game/Meshes/SM_Rider', assetType: 'StaticMesh', applied: 2, changed: 2, refused: [], saved: true })] }
  ),
  r('set_mesh_collision', 'asset', 'Set the collision of a static mesh asset: one box, sphere or capsule fitted to its bounds, its own render triangles (complex), or none; every placed copy is rebuilt to match and the mesh is saved.',
    schema({
      assetPath: str('Static mesh asset path under /Game; engine meshes are refused (duplicate one into /Game first).'),
      collisionType: { type: 'string', enum: ['box', 'sphere', 'capsule', 'complex', 'none'], description: 'box, sphere or capsule: one shape fitted to the mesh bounds (a capsule stands upright along Z) replaces its simple collision. complex: the render triangles are the collision, exact for terrain-like or concave meshes but costlier. none: no collision, so pawns and traces pass through.' },
      save: bool('Save the mesh after the change. Defaults to true.'),
    }, ['assetPath', 'collisionType']),
    schema({
      success: bool('Operation succeeded.'),
      assetPath: ASSET_PATH,
      collisionType: str('The collision the mesh has now.'),
      shapeCount: num('Simple collision shapes the mesh holds after the call (0 for complex and none).'),
      componentsRefreshed: num('Placed copies of the mesh in open worlds whose collision was rebuilt to match.'),
      saved: bool('Whether the mesh was saved.'),
      details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' },
    }, ['success']),
    WRITE, WRITE_POLICY, MEDIUM,
    { topics: ['mesh collision', 'static mesh collision', 'add box collision', 'complex as simple', 'remove collision', 'player falls through mesh', 'walk on imported mesh'],
      whenToUse: [
        'An imported or Fab mesh has no simple collision, so pawns fall through it or walk through it.',
        'A decorative mesh (grass, flowers, small debris) must stop blocking the player.',
        'A terrain-like or concave mesh must collide exactly with its own triangles (complex).',
      ],
      whenNotToUse: [
        'A dynamic mesh actor needs collision generated (use manage_geometry.configure_mesh_collision).',
        'Only one placed copy should stop colliding (use control_actor.set_actor_collision).',
        'It must be checked whether a placed mesh blocks at all (use inspect.raycast_world).',
      ],
      examples: [ex('Make an imported rock standable with one box', { assetPath: '/Game/Meshes/SM_Rock', collisionType: 'box' }, { success: true, assetPath: '/Game/Meshes/SM_Rock', collisionType: 'box', shapeCount: 1, componentsRefreshed: 3, saved: true })] }
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
  r('nanite_rebuild_mesh', 'asset', 'Turn Nanite on for a static mesh asset and set the share of its triangles Nanite keeps; the mesh is rebuilt and saved, and the reply reads naniteEnabled and trianglePercent back from it.',
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
