#!/usr/bin/env node
/**
 * Asset Management Test Suite
 * Tool: manage_asset
 * Actions: list, import, create_material, duplicate, rename, move, delete
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'List assets in /Game directory',
    toolName: 'manage_asset',
    arguments: {
      action: 'list',
      directory: '/Game'
    },
    expected: 'returns list of assets'
  },
  {
    scenario: 'List assets in /Game/Meshes',
    toolName: 'manage_asset',
    arguments: {
      action: 'list',
      directory: '/Game/Meshes'
    },
    expected: 'returns assets'
  },
  {
    scenario: 'Import FBX model',
    toolName: 'manage_asset',
    arguments: {
      action: 'import',
      sourcePath: 'C:\\Test\\model.fbx',
      destinationPath: '/Game/Meshes'
    },
    expected: 'success - asset imported'
  },
  {
    scenario: 'Import PNG texture',
    toolName: 'manage_asset',
    arguments: {
      action: 'import',
      sourcePath: 'C:\\Test\\texture.png',
      destinationPath: '/Game/Textures'
    },
    expected: 'success - texture imported'
  },
  {
    scenario: 'Create material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_TestMaterial',
      path: '/Game/Materials'
    },
    expected: 'success - material created'
  },
  {
    scenario: 'Duplicate asset',
    toolName: 'manage_asset',
    arguments: {
      action: 'duplicate',
      sourcePath: '/Game/Meshes/SM_Cube',
      destinationPath: '/Game/Meshes/Duplicates',
      save: true
    },
    expected: 'success - asset duplicated'
  },
  {
    scenario: 'Rename asset',
    toolName: 'manage_asset',
    arguments: {
      action: 'rename',
      assetPath: '/Game/Meshes/OldName',
      newName: 'NewName'
    },
    expected: 'success - asset renamed'
  },
  {
    scenario: 'Move asset to new folder',
    toolName: 'manage_asset',
    arguments: {
      action: 'move',
      assetPath: '/Game/Meshes/SM_Cube',
      destinationPath: '/Game/NewFolder',
      fixupRedirectors: true
    },
    expected: 'success - asset moved'
  },
  {
    scenario: 'Delete single asset',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete',
      assetPath: '/Game/Temp/TempAsset'
    },
    expected: 'success - asset deleted'
  },
  {
    scenario: 'Delete multiple assets',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete',
      assetPaths: ['/Game/Temp/Asset1', '/Game/Temp/Asset2']
    },
    expected: 'success - assets deleted'
  }
];

await runToolTests('Asset Management', testCases);
