#!/usr/bin/env node
// inspect logpoints: a Blueprint node marked to record its pin values each time it runs in Play, without pausing it. A
// custom event leading to one Print String, placed once, called during Play so the mark fires a known number of times.
// (tests/mcp-tools/core/logpoints.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = '/Game/_McpScratch/Logpoints';
const BP_NAME = `BP_McpLogpoint_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const ACTOR = `MCP_LogpointActor_${ts}`;

const testCases = [
  { scenario: 'Setup: a Blueprint of its own', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success' },
  // A move with an output (ReturnValue) that runs on to the print: its outputs come from the wire trace after it.
  { scenario: 'Setup: a custom event that moves the actor, then prints once', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_event', id: 'ev', customEventName: 'OnLogProbe' },
    { edit: 'create_node', id: 'move', nodeType: 'CallFunction', memberName: 'K2_SetActorLocation', pinDefaults: { NewLocation: '34000,34000,150' } },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'logpoint probe' } },
    { edit: 'connect_pins', from: '$ev.then', to: '$move.execute' },
    { edit: 'connect_pins', from: '$move.then', to: '$print.execute' },
  ] }, expected: 'success', captureResult: { key: 'logMoveId', fromField: 'result.nodeIds.move' } },
  { scenario: 'INFO: the print node id from the graph code', toolName: 'manage_blueprint', arguments: { action: 'read_graph_as_code', blueprintPath: BP_PATH, graphName: 'EventGraph' }, expected: 'success', captureResult: { key: 'logPrintId', fromField: 'result.nodes', where: { path: 'type', equals: 'Development|PrintString' }, selectField: 'id' } },
  { scenario: 'Setup: one placed copy', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: ACTOR, location: { x: 34000, y: 34000, z: 100 } }, expected: 'success' },
  { scenario: 'ACTION: mark the print by its graph-code tag', toolName: 'inspect', arguments: { action: 'set_logpoints', blueprintPath: BP_PATH, nodeIds: ['${captured:logPrintId}'], maxHits: 50 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.logpoints.0.node', includes: 'PrintString', label: 'the print is marked (its list-view title, as the graph readers give it)' },
    { path: 'structuredContent.result.logpoints.0.runs', equals: 0, label: 'it has not run yet' },
  ] },
  { scenario: 'ACTION: mark the move by its GUID', toolName: 'inspect', arguments: { action: 'set_logpoints', blueprintPath: BP_PATH, nodeIds: ['${captured:logMoveId}'] }, expected: 'success' },
  { scenario: 'ERROR: an id no node has', toolName: 'inspect', arguments: { action: 'set_logpoints', blueprintPath: BP_PATH, nodeIds: ['nzzzzzz'] }, expected: 'error|NODE_NOT_FOUND' },
  // A plain game mode: a project's own (a title screen that pauses the game or reloads the level) never steers the run.
  { scenario: 'Setup: a plain game mode for the level', toolName: 'manage_level', arguments: { action: 'set_world_settings', gameMode: '/Script/Engine.GameModeBase' }, expected: 'success' },
  { scenario: 'PLAYBACK: play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'ACTION: the event runs once in the game', toolName: 'control_actor', arguments: { action: 'call_actor_function', actorName: ACTOR, functionName: 'OnLogProbe' }, expected: 'success' },
  { scenario: 'READ: the print mark recorded the run and its input', toolName: 'inspect', arguments: { action: 'read_logpoints', nodeIds: ['${captured:logPrintId}'], last: 5 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.logpoints.0.runs', equals: 1, label: 'one run' },
    { path: 'structuredContent.result.logpoints.0.timeline.0.in.InString', equals: 'logpoint probe', label: 'the text it was given' },
    { path: 'structuredContent.result.logpoints.0.timeline.0.object', includes: ACTOR, label: 'on the placed copy' },
  ] },
  { scenario: 'READ: the move mark recorded what it returned', toolName: 'inspect', arguments: { action: 'read_logpoints', nodeIds: ['${captured:logMoveId}'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.logpoints.0.timeline.0.out.ReturnValue', exists: true, label: 'its output, read after it ran' },
    { path: 'structuredContent.result.logpoints.0.timeline.0.in.NewLocation', includes: '34000', label: 'and the location it was given' },
  ] },
  { scenario: 'PLAYBACK: stop', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'READ: the timeline stays readable after Play', toolName: 'inspect', arguments: { action: 'read_logpoints', nodeIds: ['${captured:logPrintId}'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.logpoints.0.runs', equals: 1, label: 'still one run' },
  ] },
  { scenario: 'ACTION: clear the Blueprint\'s marks', toolName: 'inspect', arguments: { action: 'clear_logpoints', blueprintPath: BP_PATH }, expected: 'success', assertions: [
    { path: 'structuredContent.result.cleared', equals: 2, label: 'both marks removed' },
  ] },
  { scenario: 'READ: nothing is marked any more', toolName: 'inspect', arguments: { action: 'read_logpoints', blueprintPath: BP_PATH }, expected: 'success', assertions: [
    { path: 'structuredContent.result.logpoints', length: 0, label: 'no marks' },
  ] },
  { scenario: 'Cleanup: the placed copy', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR }, expected: 'success' },
  { scenario: 'Cleanup: the Blueprint', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
];

runToolTests('logpoints', testCases, {});
