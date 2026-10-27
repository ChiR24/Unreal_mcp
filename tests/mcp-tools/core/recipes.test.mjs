#!/usr/bin/env node
// system_control save_recipe / run_recipe / list_recipes: a run_script script kept in the project under a name, with
// parameters, and run later as one checked call. Two sunk crates, a recipe that lifts every crate of a name prefix to a
// height, run with its arguments. Every name is this run's; the recipe is removed at the end.
// (tests/mcp-tools/core/recipes.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPRecipeTag_${ts}`;
const PREFIX = `MCP_RecipeCrate_${ts}`;
const NAME = `mcp_raise_${ts}`;
const SCRIPT = [
  '(let crates (control_actor.list :filter prefix))',
  '(for c crates.actors',
  '  (if (< c.location.z minZ)',
  '    (control_actor.set_transform :actorName c.name :location (obj :x c.location.x :y c.location.y :z minZ))))',
  '(return crates.count)',
].join('\n');

const testCases = [
  { scenario: 'Setup: two sunk crates', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: `${PREFIX}_A`, location: { x: 40000, y: 40000, z: -100 } },
    { actorName: `${PREFIX}_B`, location: { x: 40300, y: 40000, z: -100 } },
  ] }, expected: 'success' },
  { scenario: 'ACTION: save the recipe', toolName: 'system_control', arguments: { action: 'save_recipe', name: NAME, summary: 'Lift every crate of a name prefix to a height.', paramNames: ['prefix', 'minZ'], script: SCRIPT }, expected: 'success' },
  { scenario: 'ERROR: the name is taken', toolName: 'system_control', arguments: { action: 'save_recipe', name: NAME, paramNames: ['prefix', 'minZ'], script: SCRIPT }, expected: 'error|RECIPE_EXISTS' },
  { scenario: 'ACTION: replace it with overwrite', toolName: 'system_control', arguments: { action: 'save_recipe', name: NAME, summary: 'Lift every crate of a name prefix to a height.', paramNames: ['prefix', 'minZ'], script: SCRIPT, destructive: [], overwrite: true }, expected: 'success' },
  { scenario: 'ERROR: a name that is no file name', toolName: 'system_control', arguments: { action: 'save_recipe', name: 'bad-name', script: SCRIPT }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a script calling no capability', toolName: 'system_control', arguments: { action: 'save_recipe', name: `${NAME}_bad`, script: '(no_such_tool.no_such_action :x 1)' }, expected: 'error|UNKNOWN_CAPABILITY' },
  { scenario: 'READ: it is listed', toolName: 'system_control', arguments: { action: 'list_recipes', query: `raise ${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.recipes', length: 1, label: 'the one recipe of this run' },
  ] },
  { scenario: 'ERROR: run without its arguments', toolName: 'system_control', arguments: { action: 'run_recipe', name: NAME }, expected: 'error|RECIPE_ARGS' },
  { scenario: 'ERROR: run a recipe that does not exist', toolName: 'system_control', arguments: { action: 'run_recipe', name: `${NAME}_none` }, expected: 'error|RECIPE_NOT_FOUND' },
  { scenario: 'ACTION: run it', toolName: 'system_control', arguments: { action: 'run_recipe', name: NAME, args: { prefix: PREFIX, minZ: 50 }, maxSteps: 50, deadlineMs: 15000 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.returned', equals: 2, label: 'both crates were found' },
  ] },
  { scenario: 'READ: the crate was lifted', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: `${PREFIX}_A` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 50, tolerance: 0.5, label: 'at the height given' },
  ] },
  { scenario: 'READ: the run marked it verified', toolName: 'system_control', arguments: { action: 'list_recipes', query: `raise ${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.recipes.0.verified.engine', exists: true, label: 'the engine it ran on' },
  ] },
  // A recipe without parameters runs with no args at all.
  { scenario: 'ACTION: save a recipe without parameters', toolName: 'system_control', arguments: { action: 'save_recipe', name: `${NAME}_plain`, script: '(return 7)' }, expected: 'success' },
  { scenario: 'ACTION: run it with no args', toolName: 'system_control', arguments: { action: 'run_recipe', name: `${NAME}_plain` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.returned', equals: 7, label: 'what it returns' },
  ] },
  { scenario: 'Cleanup: remove that recipe', toolName: 'system_control', arguments: { action: 'save_recipe', name: `${NAME}_plain`, remove: true }, expected: 'success' },
  { scenario: 'Cleanup: remove the recipe', toolName: 'system_control', arguments: { action: 'save_recipe', name: NAME, remove: true }, expected: 'success' },
  { scenario: 'ERROR: remove a recipe that is gone', toolName: 'system_control', arguments: { action: 'save_recipe', name: NAME, remove: true }, expected: 'error|RECIPE_NOT_FOUND' },
  { scenario: 'Cleanup: the crates', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success|not found' },
];

runToolTests('recipes', testCases, {});
