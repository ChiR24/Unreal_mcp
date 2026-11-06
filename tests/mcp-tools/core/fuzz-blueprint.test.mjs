#!/usr/bin/env node
// control_editor fuzz_blueprint: a Blueprint actor of the running game is called through its own functions with
// edge-case inputs. A scratch Blueprint holds an unguarded divide, its guarded twin and an array read, placed beside a
// native marker far from everything else; every name is this run's.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = '/Game/MCPTest/Fuzz';
const BP_NAME = `BP_Fuzz_${ts}`;
const BP_PATH = `${FOLDER}/${BP_NAME}`;
const TAG = `MCPFuzzTag_${ts}`;
const TARGET = `MCP_FuzzTarget_${ts}`;
const MARKER = `MCP_FuzzMarker_${ts}`;
const FAR = { x: 44000, y: 44000, z: 100 };
const fn = (name, inputs, outputs) => ({ scenario: `Setup: function ${name}`, toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: name, inputs, outputs }, expected: 'success' });
// Average divides by a count nothing checks; SafeAverage is its guarded twin; Pick reads an array at an index; Locate
// asks an actor input for its location (an Accessed None when the input is None: a script exception, not a log warning).
const CODE = [
  '(fn Average (Total Count)\n  (return (/ Total Count)))',
  '(fn SafeAverage (Total Count)\n  (if (> Count 0)\n    (return (/ Total Count)))\n  (return 0.0))',
  '(fn Pick (Values Index)\n  (return (Utilities|Array|Get(acopy) Values Index)))',
  '(fn Locate (Target)\n  (return (Transformation|GetActorLocation Target)))',
].join('\n');

const testCases = [
  { scenario: 'ERROR: no game running', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: TARGET }, expected: 'error|EDITOR_STATE_MISMATCH' },
  { scenario: 'Setup: the Blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: BP_NAME, savePath: FOLDER, parentClass: 'Actor' }, expected: 'success' },
  fn('Average', [{ name: 'Total', type: 'Float' }, { name: 'Count', type: 'Int' }], [{ name: 'Result', type: 'Float' }]),
  fn('SafeAverage', [{ name: 'Total', type: 'Float' }, { name: 'Count', type: 'Int' }], [{ name: 'Result', type: 'Float' }]),
  fn('Pick', [{ name: 'Values', type: 'Array<Int>' }, { name: 'Index', type: 'Int' }], [{ name: 'Result', type: 'Int' }]),
  fn('Locate', [{ name: 'Target', type: 'Actor' }], [{ name: 'Result', type: 'Vector' }]),
  { scenario: 'Setup: the function bodies', toolName: 'manage_blueprint', arguments: { action: 'write_graph_from_code', blueprintPath: BP_PATH, code: CODE }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles' }] },
  { scenario: 'Setup: place the Blueprint', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: TARGET, location: FAR, tags: [TAG] }, expected: 'success' },
  { scenario: 'Setup: a native marker beside it', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { tags: [TAG] }, actors: [{ actorName: MARKER, classPath: '/Script/Engine.TargetPoint', location: { x: FAR.x + 300, y: FAR.y, z: FAR.z } }] }, expected: 'success' },
  // A plain game mode: a project's own (a title screen that pauses the game or reloads the level) never meddles.
  { scenario: 'Setup: a plain game mode for the level', toolName: 'manage_level', arguments: { action: 'set_world_settings', gameMode: '/Script/Engine.GameModeBase' }, expected: 'success' },
  { scenario: 'Setup: start Play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'ACTION: the unguarded divide fails at Count 0, its guarded twin never', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: TARGET, functions: ['Average', 'SafeAverage'] }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.functions.0.failed.0.inputs.Count', equals: 0, label: 'Average fails with Count 0 and nothing else changed' },
    { path: 'structuredContent.result.functions.0.failed.0.errors.0.message', includes: 'Divide by zero', label: 'the engine\'s own message' },
    { path: 'structuredContent.result.functions.0.failed.0.errors.0.nodeId', exists: true, label: 'read back to its node' },
    { path: 'structuredContent.result.functions.1.failed', length: 0, label: 'the guarded twin passes every input' },
  ] },
  { scenario: 'ACTION: by default every own function with inputs; the array read fails past its end', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', blueprintPath: BP_PATH, maxCalls: 60 }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.functions', length: 4, label: 'Average, SafeAverage, Pick and Locate' },
    { path: 'structuredContent.result.functions.2.failed.0.errors.0.message', includes: 'Attempted to access index', label: 'Pick reads past the end of an empty array' },
    { path: 'structuredContent.result.functions.3.first.Target', equals: TARGET, label: 'an actor input starts as the actor itself' },
    { path: 'structuredContent.result.functions.3.failed.0.errors.0.message', includes: 'Accessed None', label: 'Locate with None raises the script exception' },
    { path: 'structuredContent.result.target', equals: TARGET, label: 'the placed instance was called' },
  ] },
  { scenario: 'ACTION: maxCalls ends the run and says so', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: TARGET, functions: ['SafeAverage'], maxCalls: 3 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.calls', equals: 3, label: 'three calls' },
    { path: 'structuredContent.result.stopped', equals: 'budget', label: 'stopped by the budget' },
    { path: 'structuredContent.result.functions.0.planned', equals: 9, label: 'of the nine its inputs give' },
  ] },
  { scenario: 'ERROR: a function the Blueprint does not have', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: TARGET, functions: ['NoSuchFunction'] }, expected: 'error|FUNCTION_NOT_FOUND' },
  { scenario: 'ERROR: an actor that is not in the game', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: `MCP_NoSuchActor_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: a native actor has no Blueprint functions', toolName: 'control_editor', arguments: { action: 'fuzz_blueprint', actorName: MARKER }, expected: 'error|NOT_A_BLUEPRINT' },
  { scenario: 'Cleanup: stop Play', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'Cleanup: the actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success|not found' },
  { scenario: 'Cleanup: the Blueprint', toolName: 'manage_asset', arguments: { action: 'delete', path: BP_PATH, force: true }, expected: 'success|not found' },
];

runToolTests('fuzz-blueprint', testCases, {});
