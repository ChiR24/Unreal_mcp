#!/usr/bin/env node
// system_control run_script: capability calls in one request, checked whole before anything runs and rolled back on a
// failure. Three crates far from everything else, one sunk below the floor line; every name is this run's.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPScriptTag_${ts}`;
const PREFIX = `MCP_ScriptCrate_${ts}`;
const A = `${PREFIX}_A`;
const B = `${PREFIX}_B`;
const C = `${PREFIX}_C`;
const ROW = `MCP_ScriptRow_${ts}_`;
const run = (script, extra = {}) => ({ action: 'run_script', script, ...extra });

const RAISE_SUNK = `(let crates (control_actor.list :filter "${PREFIX}"))
(for c crates.actors
  (if (< c.location.z 0)
    (control_actor.set_transform :actorName c.label :location (obj :x c.location.x :y c.location.y :z 50))))
(for c crates.actors (expect (+ c.label ".location.z in 0..100")))
(return crates.count)`;

const testCases = [
  { scenario: 'Setup: three crates, B sunk below z 0', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [{ actorName: A, location: { x: 31000, y: 31000, z: 50 } }, { actorName: B, location: { x: 31200, y: 31000, z: -100 } }, { actorName: C, location: { x: 31400, y: 31000, z: 50 } }] }, expected: 'success' },
  { scenario: 'UPDATE: a script raises only the sunk crate and returns how many it found', toolName: 'system_control', arguments: run(RAISE_SUNK, { maxSteps: 50, deadlineMs: 15000 }), expected: 'success', assertions: [
    { path: 'structuredContent.result.returned', equals: 3, label: 'three crates listed' },
    { path: 'structuredContent.result.steps.length', equals: 2, label: 'one list and one move' },
  ] },
  { scenario: 'READ: the sunk crate stands at z 50', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: B }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 50, tolerance: 1, label: 'raised' },
  ] },
  { scenario: 'CREATE: a script spawns a row with range, joined names and computed positions', toolName: 'system_control', arguments: run(`(for i (range 2)
  (control_actor.spawn :classPath "/Engine/BasicShapes/Cube" :actorName (+ "${ROW}" i) :location (obj :x (+ 31000 (* i 200)) :y 31600 :z 50) :tags (list "${TAG}")))
(return (+ "made " 2))`), expected: 'success', assertions: [
    { path: 'structuredContent.result.returned', equals: 'made 2', label: 'the returned text' },
  ] },
  { scenario: 'READ: the second row crate is where the script put it', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: `${ROW}1` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.0', approximately: 31200, tolerance: 1, label: 'x from (+ 31000 (* i 200))' },
  ] },
  { scenario: 'ERROR: a failed step rolls back the move before it', toolName: 'system_control', arguments: run(`(control_actor.set_transform :actorName "${A}" :location (obj :x 31000 :y 31000 :z 900))
(control_actor.set_transform :actorName "MCP_NoSuchActor_${ts}" :location (obj :x 0 :y 0 :z 0))`), expected: 'error|not found' },
  { scenario: 'READ: crate A did not move', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: A }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 50, tolerance: 1, label: 'rolled back' },
  ] },
  { scenario: 'ERROR: a spawn is taken back when a later step fails', toolName: 'system_control', arguments: run(`(control_actor.spawn :classPath "/Engine/BasicShapes/Cube" :actorName "${PREFIX}_Gone" :location (obj :x 31600 :y 31000 :z 50) :tags (list "${TAG}"))
(control_actor.set_transform :actorName "MCP_NoSuchActor_${ts}" :location (obj :x 0 :y 0 :z 0))`), expected: 'error|not found' },
  { scenario: 'READ: the spawned crate is gone again', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: `${PREFIX}_Gone` }, expected: 'error|not found' },
  { scenario: 'ERROR: a spawn batch is taken back whole when a later step fails', toolName: 'system_control', arguments: run(`(control_actor.spawn :actors (list (obj :actorName "${PREFIX}_BatchA" :classPath "/Engine/BasicShapes/Cube" :location (obj :x 31800 :y 31000 :z 50)) (obj :actorName "${PREFIX}_BatchB" :classPath "/Engine/BasicShapes/Cube" :location (obj :x 32000 :y 31000 :z 50))))
(control_actor.set_transform :actorName "MCP_NoSuchActor_${ts}" :location (obj :x 0 :y 0 :z 0))`), expected: 'error|not found' },
  { scenario: 'READ: the batch\'s second crate is gone too', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: `${PREFIX}_BatchB` }, expected: 'error|not found' },
  // Each round makes one call and a thousand sums, so the deadline falls between calls on any level: 900 bare list calls
  // finished inside 100 ms on a near-empty one.
  { scenario: 'ERROR: past deadlineMs no further call starts', toolName: 'system_control', arguments: run(`(for i (range 900)
  (control_actor.list :filter "x")
  (for j (range 1000) (let k (+ i j))))`, { maxSteps: 1000, deadlineMs: 100 }), expected: 'error|SCRIPT_DEADLINE' },
  { scenario: 'ERROR: a loop of plain statements stops at deadlineMs too', toolName: 'system_control', arguments: run('(for i (range 1000) (for j (range 1000) (let k (+ i j))))', { deadlineMs: 100 }), expected: 'error|SCRIPT_DEADLINE' },
  { scenario: 'ERROR: an expectation that fails rolls the script back', toolName: 'system_control', arguments: run(`(control_actor.set_transform :actorName "${A}" :location (obj :x 31000 :y 31000 :z 700))
(expect "${A}.location.z < 100")`), expected: 'error|EXPECTATION_FAILED' },
  { scenario: 'READ: crate A is back at z 50', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: A }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 50, tolerance: 1, label: 'rolled back' },
  ] },
  { scenario: 'ERROR: a capability that asks for consent must be listed', toolName: 'system_control', arguments: run(`(control_actor.delete :actorName "${C}")`), expected: 'error|CONSENT_NOT_LISTED' },
  { scenario: 'DELETE: listed in destructive, the delete runs', toolName: 'system_control', arguments: run(`(control_actor.delete :actorName "${C}")`, { destructive: ['control_actor.delete'] }), expected: 'success' },
  { scenario: 'READ: crate C is gone', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: C }, expected: 'error|not found' },
  { scenario: 'ERROR: unbalanced parentheses', toolName: 'system_control', arguments: run('(control_actor.list :filter "x"'), expected: 'error|SCRIPT_PARSE_ERROR' },
  { scenario: 'ERROR: a capability search does not know', toolName: 'system_control', arguments: run('(control_actor.teleport_everything)'), expected: 'error|UNKNOWN_CAPABILITY' },
  { scenario: 'ERROR: a parameter the contract does not declare', toolName: 'system_control', arguments: run('(control_actor.list :colour "red")'), expected: 'error|UNDECLARED_PARAMETER' },
  { scenario: 'ERROR: a name used before it is bound', toolName: 'system_control', arguments: run('(control_actor.get_transform :actorName nobody.label)'), expected: 'error|SCRIPT_UNBOUND_NAME' },
  { scenario: 'ERROR: undo cannot run inside a script', toolName: 'system_control', arguments: run('(control_editor.undo)'), expected: 'error|SCRIPT_STEP_REFUSED' },
  { scenario: 'ERROR: a path into a result field it does not have', toolName: 'system_control', arguments: run(`(let r (control_actor.list :filter "${PREFIX}"))
(control_actor.get_transform :actorName r.nothing.label)`), expected: 'error|SCRIPT_VALUE' },
  { scenario: 'ERROR: for over a value that is not a list', toolName: 'system_control', arguments: run('(for c 5 (control_actor.list :filter "x"))'), expected: 'error|SCRIPT_TYPE' },
  { scenario: 'ERROR: past maxSteps the script stops', toolName: 'system_control', arguments: run(`(control_actor.list :filter "${PREFIX}")
(control_actor.list :filter "${PREFIX}")`, { maxSteps: 1 }), expected: 'error|SCRIPT_BUDGET' },
  { scenario: 'UPDATE: a crate a script leaves floating is named in the script\'s warnings', toolName: 'system_control', arguments: run(`(control_actor.set_transform :actorName "${A}" :location (obj :x 31000 :y 31000 :z 400))`), expected: 'success', assertions: [
    { path: 'structuredContent.receipt.warnings.0', includes: A, label: 'placement is checked once for the script, never lost in a step reply' },
  ] },
  { scenario: 'DELETE: the script scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('run-script', testCases, {});
