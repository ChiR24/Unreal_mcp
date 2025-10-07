#!/usr/bin/env node
/**
 * Environment Building Test Suite
 * Tool: build_environment
 * Actions: create_landscape, sculpt, add_foliage, paint_foliage, create_procedural_terrain, create_procedural_foliage, add_foliage_instances, create_landscape_grass_type
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'create_landscape',
      name: 'MainLandscape',
      sizeX: 8,
      sizeY: 8
    },
    expected: 'success - landscape created'
  },
  {
    scenario: 'Sculpt landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'sculpt',
      name: 'MainLandscape',
      tool: 'Sculpt'
    },
    expected: 'success - landscape sculpted'
  },
  {
    scenario: 'Smooth landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'sculpt',
      name: 'MainLandscape',
      tool: 'Smooth'
    },
    expected: 'success - landscape smoothed'
  },
  {
    scenario: 'Add foliage type',
    toolName: 'build_environment',
    arguments: {
      action: 'add_foliage',
      meshPath: '/Game/Foliage/SM_Tree',
      density: 1.0
    },
    expected: 'success - foliage added'
  },
  {
    scenario: 'Paint foliage',
    toolName: 'build_environment',
    arguments: {
      action: 'paint_foliage',
      foliageType: 'SM_Tree',
      position: { x: 0, y: 0, z: 0 },
      brushSize: 1000,
      strength: 0.5
    },
    expected: 'success - foliage painted'
  },
  {
    scenario: 'Create procedural terrain',
    toolName: 'build_environment',
    arguments: {
      action: 'create_procedural_terrain',
      name: 'ProceduralTerrain',
      location: { x: 5000, y: 0, z: 0 },
      subdivisions: 64
    },
    expected: 'success - procedural terrain created'
  },
  {
    scenario: 'Create procedural foliage',
    toolName: 'build_environment',
    arguments: {
      action: 'create_procedural_foliage',
      bounds: {
        location: { x: 0, y: 0, z: 0 },
        size: { x: 10000, y: 10000, z: 1000 }
      },
      seed: 12345
    },
    expected: 'success - procedural foliage created'
  },
  {
    scenario: 'Flatten landscape area',
    toolName: 'build_environment',
    arguments: {
      action: 'sculpt',
      name: 'MainLandscape',
      tool: 'Flatten'
    },
    expected: 'success - landscape flattened'
  },
  {
    scenario: 'Erosion tool on landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'sculpt',
      name: 'MainLandscape',
      tool: 'Erosion'
    },
    expected: 'success - erosion applied'
  },
  {
    scenario: 'Add grass foliage',
    toolName: 'build_environment',
    arguments: {
      action: 'add_foliage',
      meshPath: '/Game/Foliage/SM_Grass',
      density: 2.0
    },
    expected: 'success - grass foliage added'
  }
];

await runToolTests('Environment Building', testCases);
