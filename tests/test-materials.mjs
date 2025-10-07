#!/usr/bin/env node
/**
 * Material Test Suite (uses build_environment for materials)
 * Tool: manage_asset (create_material action)
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_BaseMaterial',
      path: '/Game/Materials'
    },
    expected: 'success - material created'
  },
  {
    scenario: 'Create PBR material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_PBRMetal',
      path: '/Game/Materials/PBR'
    },
    expected: 'success - PBR material created'
  },
  {
    scenario: 'Create glass material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Glass',
      path: '/Game/Materials/Transparent'
    },
    expected: 'success - glass material created'
  },
  {
    scenario: 'Create emissive material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Emissive',
      path: '/Game/Materials/Lights'
    },
    expected: 'success - emissive material created'
  },
  {
    scenario: 'Create water material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Water',
      path: '/Game/Materials/Environment'
    },
    expected: 'success - water material created'
  },
  {
    scenario: 'Create foliage material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Foliage',
      path: '/Game/Materials/Nature'
    },
    expected: 'success - foliage material created'
  },
  {
    scenario: 'Create stone material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Stone',
      path: '/Game/Materials/Terrain'
    },
    expected: 'success - stone material created'
  },
  {
    scenario: 'Create hologram material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Hologram',
      path: '/Game/Materials/VFX'
    },
    expected: 'success - hologram material created'
  },
  {
    scenario: 'Create subsurface skin material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_Skin',
      path: '/Game/Materials/Characters'
    },
    expected: 'success - skin material created'
  },
  {
    scenario: 'Create two-sided material',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_material',
      name: 'M_TwoSided',
      path: '/Game/Materials/Special'
    },
    expected: 'success - two-sided material created'
  }
];

await runToolTests('Materials', testCases);
