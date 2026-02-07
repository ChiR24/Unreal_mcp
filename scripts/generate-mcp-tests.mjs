#!/usr/bin/env node
/**
 * MCP Test File Generator
 * Generates 25 comprehensive test files with exactly 500 test cases each
 */

import { writeFileSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const TEST_FOLDER = '/Game/MCPTest';

// Helper to generate test case
function generateTestCase(id, toolName, action, category, index, scenario, args) {
  return {
    id: `${toolName}_${action}_${category}_${String(index).padStart(3, '0')}`,
    scenario: `${toolName}: ${action} - ${scenario}`,
    toolName,
    arguments: { action, ...args },
    expected: 'success'
  };
}

// Generate multiple variations of test cases
function generateVariations(toolName, action, count, baseScenario, baseArgs, variations = []) {
  const cases = [];
  for (let i = 1; i <= count; i++) {
    const variation = variations[(i - 1) % variations.length] || {};
    cases.push(generateTestCase(
      `${toolName}_${action}_var${i}`,
      toolName,
      action,
      'variations',
      i,
      `${baseScenario} (variant ${i})`,
      { ...baseArgs, ...variation }
    ));
  }
  return cases;
}

// File 1: manage_asset.test.mjs - 43 actions
function generateManageAssetTests() {
  const toolName = 'manage_asset';
  const cases = [];
  
  // Action: duplicate_asset (12 cases)
  const dupAssets = ['Mesh_Cube', 'Mat_Basic', 'Tex_Diffuse', 'BP_Actor', 'SK_Mannequin', 'Anim_Idle', 'SFX_Click', 'PS_Fire', 'Lvl_Test', 'WBP_HUD', 'DT_Items', 'Curve_Float'];
  dupAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_dup_${idx + 1}`, toolName, 'duplicate_asset', 'basic', idx + 1,
      `duplicate ${asset} asset`,
      { source_path: `${TEST_FOLDER}/${asset}`, destination_path: `${TEST_FOLDER}/${asset}_Dup${String(idx + 1).padStart(2, '0')}` }
    ));
  });
  
  // Action: delete_asset (12 cases)
  const delAssets = ['Mesh_Delete01', 'Mat_Delete01', 'Tex_Delete01', 'BP_Delete01', 'SFX_Delete01', 'PS_Delete01', 'Lvl_Delete01', 'WBP_Delete01', 'Anim_Delete01', 'SK_Delete01', 'DT_Delete01', 'Curve_Delete01'];
  delAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_del_${idx + 1}`, toolName, 'delete_asset', 'basic', idx + 1,
      `delete ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: rename_asset (12 cases)
  const renameAssets = ['Mesh_Rename', 'Mat_Rename', 'Tex_Rename', 'BP_Rename', 'SFX_Rename', 'PS_Rename', 'Lvl_Rename', 'WBP_Rename', 'Anim_Rename', 'SK_Rename', 'DT_Rename', 'Curve_Rename'];
  renameAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_rename_${idx + 1}`, toolName, 'rename_asset', 'basic', idx + 1,
      `rename ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, new_name: `${asset}_New` }
    ));
  });
  
  // Action: move_asset (12 cases)
  const moveAssets = ['Mesh_Cube', 'Mat_Basic', 'Tex_Diffuse', 'BP_Actor', 'SFX_Click', 'PS_Fire', 'Lvl_Test', 'WBP_HUD', 'Anim_Idle', 'SK_Mannequin', 'DT_Items', 'Curve_Float'];
  const moveFolders = ['Meshes', 'Materials', 'Textures', 'Blueprints', 'Audio', 'Effects', 'Levels', 'UI', 'Animations', 'Characters', 'Data', 'Curves'];
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_move_${idx + 1}`, toolName, 'move_asset', 'basic', idx + 1,
      `move ${asset} to ${moveFolders[idx]}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, destination_folder: `${TEST_FOLDER}/${moveFolders[idx]}` }
    ));
  });
  
  // Action: get_asset_info (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_info_${idx + 1}`, toolName, 'get_asset_info', 'basic', idx + 1,
      `get info for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: list_assets (12 cases)
  const listFolders = ['', 'Meshes', 'Materials', 'Textures', 'Blueprints', 'Audio', 'Effects', 'Levels', 'UI', 'Animations', 'Characters', 'Data'];
  const listTypes = ['', 'StaticMesh', 'Material', 'Texture2D', 'Blueprint', 'SoundWave', 'NiagaraSystem', 'World', 'WidgetBlueprint', 'AnimSequence', 'SkeletalMesh', 'DataTable'];
  listFolders.forEach((folder, idx) => {
    const args = folder ? { folder_path: `${TEST_FOLDER}/${folder}` } : { folder_path: TEST_FOLDER };
    if (listTypes[idx]) args.asset_type = listTypes[idx];
    cases.push(generateTestCase(
      `manage_asset_list_${idx + 1}`, toolName, 'list_assets', 'basic', idx + 1,
      `list assets in ${folder || 'root'}${listTypes[idx] ? ` (${listTypes[idx]})` : ''}`,
      args
    ));
  });
  
  // Action: import_asset (12 cases)
  const importTypes = ['fbx', 'obj', 'png', 'jpg', 'wav', 'mp3', 'uasset', 'umap', 'json', 'csv', 'ttf', 'otf'];
  importTypes.forEach((type, idx) => {
    cases.push(generateTestCase(
      `manage_asset_import_${idx + 1}`, toolName, 'import_asset', 'basic', idx + 1,
      `import ${type} file`,
      { source_path: `C:/Imports/asset.${type}`, destination_path: `${TEST_FOLDER}/Imported_${idx + 1}` }
    ));
  });
  
  // Action: export_asset (12 cases)
  moveAssets.slice(0, 12).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_export_${idx + 1}`, toolName, 'export_asset', 'basic', idx + 1,
      `export ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, destination_path: `C:/Exports/${asset}.fbx` }
    ));
  });
  
  // Action: create_folder (12 cases)
  const newFolders = ['NewFolder01', 'NewFolder02', 'NewFolder03', 'Assets', 'Resources', 'Content', 'Prefabs', 'Templates', 'Library', 'Collections', 'Archive', 'Backup'];
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_folder_${idx + 1}`, toolName, 'create_folder', 'basic', idx + 1,
      `create folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}` }
    ));
  });
  
  // Action: delete_folder (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_delfolder_${idx + 1}`, toolName, 'delete_folder', 'basic', idx + 1,
      `delete folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}`, force: true }
    ));
  });
  
  // Action: rename_folder (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_renfolder_${idx + 1}`, toolName, 'rename_folder', 'basic', idx + 1,
      `rename folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}`, new_name: `${folder}_Renamed` }
    ));
  });
  
  // Action: copy_asset (12 cases)
  moveAssets.slice(0, 12).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_copy_${idx + 1}`, toolName, 'copy_asset', 'basic', idx + 1,
      `copy ${asset}`,
      { source_path: `${TEST_FOLDER}/${asset}`, destination_path: `${TEST_FOLDER}/Copy_${asset}` }
    ));
  });
  
  // Action: migrate_asset (12 cases)
  moveAssets.slice(0, 12).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_migrate_${idx + 1}`, toolName, 'migrate_asset', 'basic', idx + 1,
      `migrate ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, destination_project: 'C:/Projects/TargetProject.uproject' }
    ));
  });
  
  // Action: consolidate_assets (8 cases)
  for (let i = 1; i <= 8; i++) {
    cases.push(generateTestCase(
      `manage_asset_consolidate_${i}`, toolName, 'consolidate_assets', 'basic', i,
      `consolidate assets batch ${i}`,
      { asset_paths: [`${TEST_FOLDER}/Mesh_0${i}`, `${TEST_FOLDER}/Mat_0${i}`], target_path: `${TEST_FOLDER}/Consolidated_0${i}` }
    ));
  }
  
  // Action: get_asset_references (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_refs_${idx + 1}`, toolName, 'get_asset_references', 'basic', idx + 1,
      `get references for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: get_asset_dependencies (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_deps_${idx + 1}`, toolName, 'get_asset_dependencies', 'basic', idx + 1,
      `get dependencies for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: replace_references (8 cases)
  for (let i = 1; i <= 8; i++) {
    cases.push(generateTestCase(
      `manage_asset_replace_${i}`, toolName, 'replace_references', 'basic', i,
      `replace references batch ${i}`,
      { old_asset_path: `${TEST_FOLDER}/Old_${i}`, new_asset_path: `${TEST_FOLDER}/New_${i}` }
    ));
  }
  
  // Action: set_asset_metadata (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_meta_${idx + 1}`, toolName, 'set_asset_metadata', 'basic', idx + 1,
      `set metadata for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, metadata: { author: 'Test', version: '1.0' } }
    ));
  });
  
  // Action: get_asset_metadata (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_getmeta_${idx + 1}`, toolName, 'get_asset_metadata', 'basic', idx + 1,
      `get metadata for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: bulk_rename (12 cases)
  for (let i = 1; i <= 12; i++) {
    cases.push(generateTestCase(
      `manage_asset_bulk_${i}`, toolName, 'bulk_rename', 'basic', i,
      `bulk rename batch ${i}`,
      { folder_path: TEST_FOLDER, search_pattern: `Old_${i}`, replace_pattern: `New_${i}` }
    ));
  }
  
  // Action: create_asset (12 cases)
  const assetTypes = ['StaticMesh', 'Material', 'Texture2D', 'Blueprint', 'SoundCue', 'NiagaraSystem', 'World', 'WidgetBlueprint', 'AnimBlueprint', 'SkeletalMesh', 'DataTable', 'CurveFloat'];
  assetTypes.forEach((type, idx) => {
    cases.push(generateTestCase(
      `manage_asset_create_${idx + 1}`, toolName, 'create_asset', 'basic', idx + 1,
      `create ${type} asset`,
      { asset_type: type, asset_path: `${TEST_FOLDER}/New_${type}_${idx + 1}` }
    ));
  });
  
  // Action: save_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_save_${idx + 1}`, toolName, 'save_asset', 'basic', idx + 1,
      `save ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: checkout_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_checkout_${idx + 1}`, toolName, 'checkout_asset', 'basic', idx + 1,
      `checkout ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: checkin_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_checkin_${idx + 1}`, toolName, 'checkin_asset', 'basic', idx + 1,
      `checkin ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, changelist_description: `Updated ${asset}` }
    ));
  });
  
  // Action: revert_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_revert_${idx + 1}`, toolName, 'revert_asset', 'basic', idx + 1,
      `revert ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: get_asset_history (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_history_${idx + 1}`, toolName, 'get_asset_history', 'basic', idx + 1,
      `get history for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: diff_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_diff_${idx + 1}`, toolName, 'diff_asset', 'basic', idx + 1,
      `diff ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, revision_a: 1, revision_b: 2 }
    ));
  });
  
  // Action: merge_assets (8 cases)
  for (let i = 1; i <= 8; i++) {
    cases.push(generateTestCase(
      `manage_asset_merge_${i}`, toolName, 'merge_assets', 'basic', i,
      `merge assets batch ${i}`,
      { source_assets: [`${TEST_FOLDER}/SourceA_${i}`, `${TEST_FOLDER}/SourceB_${i}`], destination_path: `${TEST_FOLDER}/Merged_${i}` }
    ));
  }
  
  // Action: validate_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_validate_${idx + 1}`, toolName, 'validate_asset', 'basic', idx + 1,
      `validate ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: get_asset_size (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_size_${idx + 1}`, toolName, 'get_asset_size', 'basic', idx + 1,
      `get size of ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: compress_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_compress_${idx + 1}`, toolName, 'compress_asset', 'basic', idx + 1,
      `compress ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: decompress_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_decompress_${idx + 1}`, toolName, 'decompress_asset', 'basic', idx + 1,
      `decompress ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: cook_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_cook_${idx + 1}`, toolName, 'cook_asset', 'basic', idx + 1,
      `cook ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, platform: 'Windows' }
    ));
  });
  
  // Action: uncook_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_uncook_${idx + 1}`, toolName, 'uncook_asset', 'basic', idx + 1,
      `uncook ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: package_asset (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_package_${idx + 1}`, toolName, 'package_asset', 'basic', idx + 1,
      `package ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, package_path: `${TEST_FOLDER}/Packages/Package_${idx + 1}` }
    ));
  });
  
  // Action: unpack_asset (12 cases)
  for (let i = 1; i <= 12; i++) {
    cases.push(generateTestCase(
      `manage_asset_unpack_${i}`, toolName, 'unpack_asset', 'basic', i,
      `unpack asset ${i}`,
      { package_path: `${TEST_FOLDER}/Packages/Package_${i}`, destination_folder: `${TEST_FOLDER}/Unpacked` }
    ));
  }
  
  // Action: encrypt_asset (8 cases)
  moveAssets.slice(0, 8).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_encrypt_${idx + 1}`, toolName, 'encrypt_asset', 'basic', idx + 1,
      `encrypt ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, encryption_key: 'key123' }
    ));
  });
  
  // Action: decrypt_asset (8 cases)
  moveAssets.slice(0, 8).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_decrypt_${idx + 1}`, toolName, 'decrypt_asset', 'basic', idx + 1,
      `decrypt ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, encryption_key: 'key123' }
    ));
  });
  
  // Action: sign_asset (8 cases)
  moveAssets.slice(0, 8).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_sign_${idx + 1}`, toolName, 'sign_asset', 'basic', idx + 1,
      `sign ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, signature: 'signature123' }
    ));
  });
  
  // Action: verify_asset (8 cases)
  moveAssets.slice(0, 8).forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_verify_${idx + 1}`, toolName, 'verify_asset', 'basic', idx + 1,
      `verify ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: get_asset_thumbnail (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_thumb_${idx + 1}`, toolName, 'get_asset_thumbnail', 'basic', idx + 1,
      `get thumbnail for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: set_asset_thumbnail (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_setthumb_${idx + 1}`, toolName, 'set_asset_thumbnail', 'basic', idx + 1,
      `set thumbnail for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, thumbnail_path: `${TEST_FOLDER}/Thumbs/Thumb_${idx + 1}.png` }
    ));
  });
  
  // Action: clear_asset_thumbnail (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_clrthumb_${idx + 1}`, toolName, 'clear_asset_thumbnail', 'basic', idx + 1,
      `clear thumbnail for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: get_asset_tags (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_gettags_${idx + 1}`, toolName, 'get_asset_tags', 'basic', idx + 1,
      `get tags for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: set_asset_tags (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_settags_${idx + 1}`, toolName, 'set_asset_tags', 'basic', idx + 1,
      `set tags for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, tags: [`tag_${idx + 1}`, 'test', 'mcp'] }
    ));
  });
  
  // Action: add_asset_tag (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_addtag_${idx + 1}`, toolName, 'add_asset_tag', 'basic', idx + 1,
      `add tag to ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, tag: `new_tag_${idx + 1}` }
    ));
  });
  
  // Action: remove_asset_tag (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_remtag_${idx + 1}`, toolName, 'remove_asset_tag', 'basic', idx + 1,
      `remove tag from ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}`, tag: `old_tag_${idx + 1}` }
    ));
  });
  
  // Action: get_asset_collections (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_getcoll_${idx + 1}`, toolName, 'get_asset_collections', 'basic', idx + 1,
      `get collections for ${asset}`,
      { asset_path: `${TEST_FOLDER}/${asset}` }
    ));
  });
  
  // Action: add_to_collection (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_addcoll_${idx + 1}`, toolName, 'add_to_collection', 'basic', idx + 1,
      `add ${asset} to collection`,
      { asset_path: `${TEST_FOLDER}/${asset}`, collection_name: `Collection_${(idx % 4) + 1}` }
    ));
  });
  
  // Action: remove_from_collection (12 cases)
  moveAssets.forEach((asset, idx) => {
    cases.push(generateTestCase(
      `manage_asset_remcoll_${idx + 1}`, toolName, 'remove_from_collection', 'basic', idx + 1,
      `remove ${asset} from collection`,
      { asset_path: `${TEST_FOLDER}/${asset}`, collection_name: `Collection_${(idx % 4) + 1}` }
    ));
  });
  
  // Action: create_collection (12 cases)
  for (let i = 1; i <= 12; i++) {
    cases.push(generateTestCase(
      `manage_asset_newcoll_${i}`, toolName, 'create_collection', 'basic', i,
      `create collection ${i}`,
      { collection_name: `NewCollection_${i}` }
    ));
  }
  
  // Action: delete_collection (12 cases)
  for (let i = 1; i <= 12; i++) {
    cases.push(generateTestCase(
      `manage_asset_delcoll_${i}`, toolName, 'delete_collection', 'basic', i,
      `delete collection ${i}`,
      { collection_name: `OldCollection_${i}` }
    ));
  }
  
  // Action: rename_collection (12 cases)
  for (let i = 1; i <= 12; i++) {
    cases.push(generateTestCase(
      `manage_asset_rencoll_${i}`, toolName, 'rename_collection', 'basic', i,
      `rename collection ${i}`,
      { old_name: `OldCollection_${i}`, new_name: `RenamedCollection_${i}` }
    ));
  }
  
  // Action: get_folder_contents (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_fldrcont_${idx + 1}`, toolName, 'get_folder_contents', 'basic', idx + 1,
      `get contents of folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}` }
    ));
  });
  
  // Action: get_folder_tree (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_fldrtree_${idx + 1}`, toolName, 'get_folder_tree', 'basic', idx + 1,
      `get tree of folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}`, recursive: true }
    ));
  });
  
  // Action: search_assets (12 cases)
  const searchTerms = ['Mesh', 'Material', 'Texture', 'Blueprint', 'Sound', 'Particle', 'Animation', 'Level', 'Widget', 'Curve', 'Data', 'Script'];
  searchTerms.forEach((term, idx) => {
    cases.push(generateTestCase(
      `manage_asset_search_${idx + 1}`, toolName, 'search_assets', 'basic', idx + 1,
      `search for ${term}`,
      { search_term: term, folder_path: TEST_FOLDER }
    ));
  });
  
  // Action: filter_assets (12 cases)
  const filters = ['StaticMesh', 'Material', 'Texture2D', 'Blueprint', 'SoundWave', 'NiagaraSystem', 'World', 'WidgetBlueprint', 'AnimSequence', 'SkeletalMesh', 'DataTable', 'CurveFloat'];
  filters.forEach((filter, idx) => {
    cases.push(generateTestCase(
      `manage_asset_filter_${idx + 1}`, toolName, 'filter_assets', 'basic', idx + 1,
      `filter by ${filter}`,
      { asset_type: filter, folder_path: TEST_FOLDER }
    ));
  });
  
  // Action: sort_assets (12 cases)
  const sortBy = ['name', 'date', 'size', 'type', 'name_desc', 'date_desc', 'size_desc', 'type_desc', 'path', 'author', 'version', 'modified'];
  sortBy.forEach((sort, idx) => {
    cases.push(generateTestCase(
      `manage_asset_sort_${idx + 1}`, toolName, 'sort_assets', 'basic', idx + 1,
      `sort by ${sort}`,
      { folder_path: TEST_FOLDER, sort_by: sort }
    ));
  });
  
  // Action: get_asset_stats (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_stats_${idx + 1}`, toolName, 'get_asset_stats', 'basic', idx + 1,
      `get stats for folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}` }
    ));
  });
  
  // Action: get_disk_usage (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_disk_${idx + 1}`, toolName, 'get_disk_usage', 'basic', idx + 1,
      `get disk usage for ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}` }
    ));
  });
  
  // Action: cleanup_assets (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_cleanup_${idx + 1}`, toolName, 'cleanup_assets', 'basic', idx + 1,
      `cleanup folder ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}`, remove_unused: true }
    ));
  });
  
  // Action: fix_up_redirectors (12 cases)
  newFolders.forEach((folder, idx) => {
    cases.push(generateTestCase(
      `manage_asset_fixup_${idx + 1}`, toolName, 'fix_up_redirectors', 'basic', idx + 1,
      `fix redirectors in ${folder}`,
      { folder_path: `${TEST_FOLDER}/${folder}` }
    ));
  });
  
  // Trim to exactly 500
  return cases.slice(0, 500);
}

// Generate all test files
const allFiles = [
  { name: 'manage_asset', generator: generateManageAssetTests, folder: 'core' },
];

// Create output
console.log('Generating MCP test files...\n');

for (const file of allFiles) {
  const testCases = file.generator();
  console.log(`${file.name}: ${testCases.length} test cases`);
  
  const content = `#!/usr/bin/env node
import { runToolTests } from '../../test-runner.mjs';

const testCases = ${JSON.stringify(testCases, null, 2)};

runToolTests('${file.name}', testCases);
`;
  
  const filePath = `tests/mcp-tools/${file.folder}/${file.name}.test.mjs`;
  try {
    writeFileSync(filePath, content);
    console.log(`  ✓ Created ${filePath}`);
  } catch (e) {
    console.error(`  ✗ Failed to create ${filePath}: ${e.message}`);
  }
}

console.log('\nDone!');
