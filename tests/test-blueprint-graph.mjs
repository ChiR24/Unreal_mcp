#!/usr/bin/env node
/**
 * Blueprint Graph Test Suite
 * Tools: manage_blueprint, manage_blueprint_graph
 */

import { runToolTests } from './test-runner.mjs';

// Use a unique blueprint name per run to avoid collisions
const ts = new Date().toISOString().replace(/[^0-9]/g, '').slice(0, 12);
const BP_NAME = `BP_Graph_${ts}`;
const BP_PATH = `/Game/Blueprints/${BP_NAME}`;

const testCases = [
  {
    scenario: 'Create blueprint for graph tests',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create',
      name: BP_NAME,
      blueprintType: 'Actor',
      savePath: '/Game/Blueprints',
      waitForCompletion: true
    },
    expected: 'success'
  },
  {
    scenario: 'Create Literal object node via manage_blueprint_graph',
    toolName: 'manage_blueprint_graph',
    arguments: {
      action: 'create_node',
      blueprintPath: BP_PATH,
      graphName: 'EventGraph',
      nodeType: 'Literal',
      literalType: 'object',
      objectPath: '/Engine/BasicShapes/Cube.Cube',
      x: 0,
      y: 0
    },
    expected: 'success'
  },
  {
    scenario: 'Error: Literal object path not found',
    toolName: 'manage_blueprint_graph',
    arguments: {
      action: 'create_node',
      blueprintPath: BP_PATH,
      graphName: 'EventGraph',
      nodeType: 'Literal',
      literalType: 'object',
      objectPath: '/Engine/BasicShapes/DoesNotExist.DoesNotExist',
      x: 100,
      y: 0
    },
    expected: 'error|object_not_found'
  },
  {
    scenario: 'Get graph details via manage_blueprint_graph',
    toolName: 'manage_blueprint_graph',
    arguments: {
      action: 'get_graph_details',
      blueprintPath: BP_PATH,
      graphName: 'EventGraph'
    },
    expected: 'success'
  },
  {
    scenario: 'Error: Graph not found for manage_blueprint_graph',
    toolName: 'manage_blueprint_graph',
    arguments: {
      action: 'get_graph_details',
      blueprintPath: BP_PATH,
      graphName: 'InvalidGraphName_DOES_NOT_EXIST'
    },
    expected: 'error|graph_not_found'
  }
];

await runToolTests('BlueprintGraph', testCases);
