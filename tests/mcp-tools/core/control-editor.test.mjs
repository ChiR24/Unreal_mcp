#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const FOCUS_ACTOR = `MCP_EditorFocus_${ts}`;
const PIE_PAWN = `MCP_EditorPawn_${ts}`;
const BP_NAME = `BP_ControlEditor_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const SCREENSHOT_NAME = `MCP_ControlEditor_${ts}`;

const cameraLocation = { x: 250, y: -350, z: 260 };
const cameraRotation = { pitch: -20, yaw: 35, roll: 0 };

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create focus actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: FOCUS_ACTOR, location: { x: 0, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: create PIE pawn', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DefaultPawn', actorName: PIE_PAWN, location: { x: 180, y: 0, z: 140 } }, expected: 'success|already exists' },
  { scenario: 'Setup: create asset for editor open/close', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },

  // === PLAYBACK / PIE STATE ===
  { scenario: 'PLAYBACK: play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success' },
  // propertyNames are read on the actor itself as well as on its components, and a name found nowhere is listed.
  { scenario: 'INFO: runtime_report reads the actor\'s own properties while the game runs', toolName: 'inspect', arguments: { action: 'runtime_report', actorName: PIE_PAWN, propertyNames: ['InitialLifeSpan', 'NoSuchProperty'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.properties.InitialLifeSpan', equals: 0, label: 'read on the actor' }, { path: 'structuredContent.result.propertiesNotFound.0', equals: 'NoSuchProperty', label: 'the unknown name is listed once' }] },
  { scenario: 'ACTION: set_view_target', toolName: 'control_editor', arguments: { action: 'set_view_target', actorName: FOCUS_ACTOR, blendTime: 0 }, expected: 'success' },
  { scenario: 'ACTION: set_game_view_target objectPath', toolName: 'control_editor', arguments: { action: 'set_game_view_target', objectPath: FOCUS_ACTOR, blendTime: 0.1 }, expected: 'success' },
  { scenario: 'ACTION: possess', toolName: 'control_editor', arguments: { action: 'possess', actorName: PIE_PAWN }, expected: 'success|NOT_IN_PIE' },
  // The reply leaves while a held key is still down, so it names the pawn to read once the key is released.
  { scenario: 'ACTION: apply_damage hits an actor in the running game', toolName: 'manage_combat', arguments: { action: 'apply_damage', actorName: PIE_PAWN, damage: 10, causerName: FOCUS_ACTOR, damageType: '/Script/Engine.DamageType' }, expected: 'success', assertions: [{ path: 'structuredContent.result.damageApplied', equals: 10, label: 'the pawn took the hit' }] },
  // One impulse, not a one-frame force (60000 used to move a crate 0 cm): the reply says what it moved and how fast.
  { scenario: 'ACTION: apply_force pushes a body in the running game', toolName: 'control_actor', arguments: { action: 'apply_force', actorName: FOCUS_ACTOR, force: [0, 0, 30000] }, expected: 'success', assertions: [{ path: 'structuredContent.result.massKg', gte: 1, label: 'the mass pushed is named' }, { path: 'structuredContent.result.velocityChange.2', gte: 1, label: 'with the upward speed the push gave it' }] },
  // While a game runs, x and y are pixels of its viewport: a point outside it (or a viewport with no size) is refused.
  { scenario: 'ERROR: simulate_input mouse_move outside the game viewport', toolName: 'control_editor', arguments: { action: 'simulate_input', type: 'mouse_move', x: -5, y: -5 }, expected: 'error' },
  { scenario: 'ACTION: simulate_input in PIE names the pawn the key drives', toolName: 'control_editor', arguments: { action: 'simulate_input', type: 'key_tap', key: 'W' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pawn', equals: PIE_PAWN, label: 'the possessed pawn is named' }] },
  // The game draws its view target's camera until the player is ejected: there is no free camera to move, and the refusal names the fix.
  { scenario: 'ERROR: set_camera while the player is not ejected', toolName: 'control_editor', arguments: { action: 'set_camera', location: cameraLocation, rotation: cameraRotation }, expected: 'error|PIE_VIEW_NOT_EJECTED' },
  { scenario: 'ERROR: screenshot from a camera while the player is not ejected', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'editor_viewport', resolution: '320x180', location: cameraLocation, rotation: cameraRotation }, expected: 'error|PIE_VIEW_NOT_EJECTED' },
  // The other level-viewport moves and toggles refuse the same way, and the view settings reach the game on screen.
  { scenario: 'ERROR: focus_actor while the player is not ejected', toolName: 'control_editor', arguments: { action: 'focus_actor', actorName: FOCUS_ACTOR }, expected: 'error|PIE_VIEW_NOT_EJECTED' },
  { scenario: 'ERROR: set_game_view while the player is not ejected', toolName: 'control_editor', arguments: { action: 'set_game_view', enabled: true }, expected: 'error|PIE_VIEW_NOT_EJECTED' },
  { scenario: 'ACTION: set_view_mode reaches the running game', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success', assertions: [{ path: 'structuredContent.result.method', equals: 'game_viewport', label: 'the game view took the mode' }] },
  { scenario: 'ACTION: set_camera_fov reaches the running game', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 90 }, expected: 'success', assertions: [{ path: 'structuredContent.result.method', equals: 'player_camera_manager', label: 'the game camera took the FOV' }] },
  { scenario: 'ERROR: possess with no pawn while the player is not ejected', toolName: 'control_editor', arguments: { action: 'possess' }, expected: 'error|INVALID_ARGUMENT' },
  // Replays record the running game, so they are exercised while PIE runs.
  { scenario: 'ACTION: start_recording', toolName: 'control_editor', arguments: { action: 'start_recording', name: `Recording_${ts}` }, expected: 'success' },
  { scenario: 'PLAYBACK: stop_recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success' },
  { scenario: 'OPTIONAL: start_recording with durationSeconds and frameRate', toolName: 'control_editor', arguments: { action: 'start_recording', name: `Recording_Opt_${ts}`, durationSeconds: 1, frameRate: 30 }, expected: 'success' },
  // durationSeconds 1 may already have stopped it, and stop_recording then says nothing records.
  { scenario: 'PLAYBACK: stop_recording after optional capture', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success|no replay is recording' },
  { scenario: 'PLAYBACK: pause', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success' },
  // A pawn teleported into a paused game floats where it was put; the reply and its receipt say why.
  { scenario: 'VERIFY: a world edit while paused says nothing will move', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: PIE_PAWN, location: { x: 180, y: 0, z: 400 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.details.gamePaused', equals: true, label: 'the reply flags the paused play world' }, { path: 'structuredContent.receipt.warnings.1', includes: 'The game is paused', label: 'and the receipt says nothing will move' }] },
  { scenario: 'VERIFY: world settings read the pause', toolName: 'inspect', arguments: { action: 'get_world_settings' }, expected: 'success', assertions: [{ path: 'structuredContent.result.isPaused', equals: true, label: 'the play world reads paused' }] },
  // A game that pauses itself (a title screen) stays paused with the editor's pause off: resume says so.
  { scenario: 'PLAYBACK: resume', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success|GAME_PAUSED' },
  { scenario: 'PLAYBACK: resume with unpauseGame', toolName: 'control_editor', arguments: { action: 'resume', unpauseGame: true }, expected: 'success' },
  { scenario: 'CONFIG: set_game_speed', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 0.5 }, expected: 'success' },
  { scenario: 'CONFIG: set_fixed_delta_time', toolName: 'control_editor', arguments: { action: 'set_fixed_delta_time', deltaTime: 0.01667 }, expected: 'success' },
  { scenario: 'CONFIG: set_fixed_delta_time off', toolName: 'control_editor', arguments: { action: 'set_fixed_delta_time', deltaTime: 0 }, expected: 'success' },
  { scenario: 'CONFIG: set_game_speed back to normal', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 1 }, expected: 'success' },
  { scenario: 'ACTION: step_frame', toolName: 'control_editor', arguments: { action: 'step_frame', steps: 1 }, expected: 'success' },
  { scenario: 'ACTION: single_frame_step', toolName: 'control_editor', arguments: { action: 'single_frame_step', steps: 1 }, expected: 'success' },
  // Ejecting is the editor's own Eject button: the player leaves its pawn and the view is a free camera, which set_camera
  // moves and the screenshot photographs. It runs last in the PIE block because the game no longer drives the pawn after it.
  { scenario: 'ACTION: eject', toolName: 'control_editor', arguments: { action: 'eject' }, expected: 'success', assertions: [{ path: 'structuredContent.result.view', equals: 'pie_ejected', label: 'the view is now a free camera' }] },
  { scenario: 'CONFIG: set_camera places the ejected view', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 0, y: -600, z: 300 }, rotation: { pitch: -15, yaw: 90, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.view', equals: 'pie_ejected', label: 'the move says which view it moved' }, { path: 'structuredContent.result.cameraLocation.y', approximately: -600, tolerance: 1, label: 'the camera is where it was put' }] },
  { scenario: 'OPTIONAL: screenshot of the ejected game view from a camera placed in the same call', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'game_viewport', resolution: '320x180', location: { x: 0, y: -700, z: 300 }, rotation: { pitch: -15, yaw: 90, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.view', equals: 'pie_ejected', label: 'the picture is of the ejected view' }, { path: 'structuredContent.result.cameraLocation.y', approximately: -700, tolerance: 1, label: 'it was taken from the given place' }] },
  // possess with no pawn is the way back: the editor's Possess button returns the player to its own pawn.
  { scenario: 'ACTION: possess brings the ejected player back', toolName: 'control_editor', arguments: { action: 'possess' }, expected: 'success', assertions: [{ path: 'structuredContent.result.returnedFromEject', equals: true, label: 'the player came back from eject' }, { path: 'structuredContent.result.view', equals: 'pie_game', label: 'the game draws the pawn camera again' }] },
  { scenario: 'PLAYBACK: stop', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success' },
  { scenario: 'PLAYBACK: stop_pie', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success' },

  // === CAMERA / VIEWPORT ===
  { scenario: 'CONFIG: set_camera', toolName: 'control_editor', arguments: { action: 'set_camera', location: cameraLocation, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_camera_position', toolName: 'control_editor', arguments: { action: 'set_camera_position', location: { x: 300, y: -320, z: 240 }, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_camera', toolName: 'control_editor', arguments: { action: 'set_viewport_camera', location: { x: 340, y: -280, z: 220 }, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_camera_fov', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 85 }, expected: 'success' },
  { scenario: 'CONFIG: set_view_mode', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success' },
  // Only ten names were known; any engine view mode now resolves by its display name, and an unknown one is refused.
  { scenario: 'CONFIG: set_view_mode by display name', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Player Collision' }, expected: 'success', assertions: [{ path: 'structuredContent.result.viewMode', equals: 'CollisionPawn', label: 'the display name finds the engine view mode' }] },
  { scenario: 'ERROR: set_view_mode with an unknown name', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Sepia' }, expected: 'error|is not a view mode' },
  { scenario: 'CONFIG: set_view_mode back to Lit', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_resolution', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 1280, height: 720 }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_realtime', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', realtime: false }, expected: 'success' },

  // === COMMANDS / CAPTURE / RECORDING ===
  { scenario: 'ACTION: console_command', toolName: 'control_editor', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'ACTION: execute_command', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'ACTION: screenshot', toolName: 'control_editor', arguments: { action: 'screenshot', filename: SCREENSHOT_NAME, resolution: '640x360', mode: 'editor_viewport', returnBase64: false, includeMetadata: true, metadata: { source: 'control-editor-suite' } }, expected: 'success' },
  { scenario: 'ACTION: take_screenshot', toolName: 'control_editor', arguments: { action: 'take_screenshot', filename: `${SCREENSHOT_NAME}_Alias`, resolution: '640x360' }, expected: 'success' },
  { scenario: 'OPTIONAL: screenshot into a chosen project directory', toolName: 'control_editor', arguments: { action: 'screenshot', filename: `${SCREENSHOT_NAME}_Path`, path: 'Saved/Screenshots/MCPTest', resolution: '320x180' }, expected: 'success' },
  { scenario: 'OPTIONAL: screenshot of a named editor window', toolName: 'control_editor', arguments: { action: 'screenshot', filename: `${SCREENSHOT_NAME}_Window`, mode: 'full_editor_window', window: '0', resolution: '640x360' }, expected: 'success' },
  { scenario: 'OPTIONAL: screenshot of a window picked by its integer index', toolName: 'control_editor', arguments: { action: 'screenshot', filename: `${SCREENSHOT_NAME}_WindowIndex`, mode: 'full_editor_window', window: 0, resolution: '320x180' }, expected: 'success', assertions: [{ path: 'structuredContent.result.windows', minLength: 1, label: 'the capture went through and lists the open windows' }] },
  { scenario: 'OPTIONAL: screenshot reports the shader jobs still compiling', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'editor_viewport', resolution: '320x180' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'the reply says how many shader jobs were outstanding' }] },
  { scenario: 'OPTIONAL: screenshot waits for the shader queue to drain', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'editor_viewport', resolution: '320x180', waitForShaders: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'the capture was taken, after any wait, and reports what was left' }] },
  { scenario: 'OPTIONAL: screenshot that leaves no file behind', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'full_editor_window', resolution: '320x180', returnBase64: true, keepFile: false }, expected: 'success' },
  { scenario: 'OPTIONAL: screenshot from a camera placed in the same call', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'editor_viewport', resolution: '320x180', location: { x: 0, y: -600, z: 300 }, rotation: { pitch: -15, yaw: 90, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.cameraLocation.y', approximately: -600, tolerance: 1, label: 'the picture was taken from the given place' }] },
  { scenario: 'OPTIONAL: keepFile false with returnBase64 false is refused', toolName: 'control_editor', arguments: { action: 'screenshot', mode: 'editor_viewport', keepFile: false, returnBase64: false }, expected: 'error|INVALID_ARGUMENT' },
  // A replay records the running game, so outside PIE there is nothing to record.
  { scenario: 'ERROR: start_recording outside PIE', toolName: 'control_editor', arguments: { action: 'start_recording', name: `Recording_NoPie_${ts}` }, expected: 'error|NO_ACTIVE_SESSION' },
  { scenario: 'ERROR: stop_recording with nothing recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'error|NOT_RECORDING' },

  // === BOOKMARKS / PREFERENCES / ASSETS ===
  { scenario: 'CREATE: create_bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', id: 0 }, expected: 'success' },
  { scenario: 'CREATE: create_bookmark slot 1', toolName: 'control_editor', arguments: { action: 'create_bookmark', id: 1 }, expected: 'success' },
  { scenario: 'ACTION: jump_to_bookmark', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', id: 0 }, expected: 'success' },
  { scenario: 'ERROR: jump_to_bookmark out of range', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', id: 99 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CONFIG: set_preferences', toolName: 'control_editor', arguments: { action: 'set_preferences', category: 'LevelEditor', preferences: { RealtimeAudio: false } }, expected: 'success' },
  // Minimize first, so the restore case below leaves the editor on screen for the cases that follow.
  { scenario: 'CONFIG: restore_editor_window with minimize puts the editor away without taking focus', toolName: 'control_editor', arguments: { action: 'restore_editor_window', minimize: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.minimized', equals: true, label: 'the main window reads minimized afterwards' }, { path: 'structuredContent.result.throttleOff', equals: false, label: 'background throttling is back on' }] },
  { scenario: 'CONFIG: restore_editor_window without taking focus', toolName: 'control_editor', arguments: { action: 'restore_editor_window', unthrottle: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.throttleOff', equals: true, label: 'background throttling is off' }] },
  { scenario: 'ACTION: open_asset', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: BP_PATH }, expected: 'success' },
  { scenario: 'ACTION: close_asset', toolName: 'control_editor', arguments: { action: 'close_asset', assetPath: BP_PATH }, expected: 'success' },
  { scenario: 'ERROR: close_asset with no editor open', toolName: 'control_editor', arguments: { action: 'close_asset', assetPath: BP_PATH }, expected: 'error|EDITOR_NOT_OPEN' },

  // === INPUT / LEVEL / ACTOR FOCUS ===
  { scenario: 'ACTION: simulate_input', toolName: 'control_editor', arguments: { action: 'simulate_input', inputAction: 'pressed', key: 'K' }, expected: 'success' },
  { scenario: 'ACTION: simulate_input mouse move', toolName: 'control_editor', arguments: { action: 'simulate_input', inputType: 'move', x: 320, y: 180 }, expected: 'success' },
  { scenario: 'ACTION: simulate_input mouse click', toolName: 'control_editor', arguments: { action: 'simulate_input', type: 'mouse_click', x: 320, y: 180, button: 'left' }, expected: 'success' },
  // Enhanced Input: a raw key never reaches an InputAction, so an action path
  // is injected instead. Without PIE running there is nothing to inject into.
  { scenario: 'ACTION: simulate_input enhanced input action', toolName: 'control_editor', arguments: { action: 'simulate_input', inputAction: '/Game/Input/IA_Jump', value: 1, holdSeconds: 0.1 }, expected: 'success|not found|NO_PIE' },
  { scenario: 'ACTION: simulate_input Axis3D action value', toolName: 'control_editor', arguments: { action: 'simulate_input', inputAction: '/Game/Input/IA_Move', x: 1, y: 0, z: 0.5 }, expected: 'success|not found|NO_PIE' },
  // Live UMG is driven by reflection, not the cursor; outside PIE nothing is on screen.
  { scenario: 'ACTION: simulate_input widget click', toolName: 'control_editor', arguments: { action: 'simulate_input', type: 'widget_click', widget: 'PlayButton' }, expected: 'success|No live widget' },
  { scenario: 'ACTION: focus_actor', toolName: 'control_editor', arguments: { action: 'focus_actor', actorName: FOCUS_ACTOR }, expected: 'success' },
  // set_selected_actors: focus_actor left the focus actor selected.
  { scenario: 'ACTION: set_selected_actors replaces the selection', toolName: 'control_editor', arguments: { action: 'set_selected_actors', actorNames: [PIE_PAWN] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.selected.0', equals: PIE_PAWN, label: 'the pawn is selected' },
    { path: 'structuredContent.result.selectedCount', equals: 1, label: 'and nothing else' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'ACTION: undo puts the earlier selection back', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'READ: set_selected_actors adding what is selected changes nothing', toolName: 'control_editor', arguments: { action: 'set_selected_actors', actorNames: [FOCUS_ACTOR], mode: 'add' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.selectedCount', equals: 1, label: 'the focus actor alone, as undo left it' },
    { path: 'structuredContent.result.undo', exists: false, label: 'and no undo step' },
  ] },
  { scenario: 'ACTION: set_selected_actors adds to the selection', toolName: 'control_editor', arguments: { action: 'set_selected_actors', actorNames: [PIE_PAWN], mode: 'add' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.selectedCount', equals: 2, label: 'both selected' },
  ] },
  { scenario: 'ACTION: set_selected_actors takes one off the selection', toolName: 'control_editor', arguments: { action: 'set_selected_actors', actorNames: [FOCUS_ACTOR], mode: 'remove' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.selected.0', equals: PIE_PAWN, label: 'the pawn stays' },
    { path: 'structuredContent.result.selectedCount', equals: 1, label: 'alone' },
  ] },
  { scenario: 'ACTION: set_selected_actors with no names clears the selection', toolName: 'control_editor', arguments: { action: 'set_selected_actors' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.selectedCount', equals: 0, label: 'nothing selected' },
  ] },
  { scenario: 'ERROR: set_selected_actors names an actor that is not there', toolName: 'control_editor', arguments: { action: 'set_selected_actors', actorNames: [`MCP_NoSuchActor_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: set_selected_actors add with no names', toolName: 'control_editor', arguments: { action: 'set_selected_actors', mode: 'add' }, expected: 'error|INVALID_ARGUMENT' },

  // === EDITOR DISPLAY / MODE / HISTORY ===
  // The game clock exists only while PIE runs; in edit mode set_game_speed used to
  // write Time Dilation into the level's own World Settings.
  { scenario: 'ERROR: set_game_speed outside PIE', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 1 }, expected: 'error|NO_ACTIVE_SESSION|EDITOR_STATE_MISMATCH' },
  { scenario: 'ACTION: show_stats', toolName: 'control_editor', arguments: { action: 'show_stats', stat: 'fps' }, expected: 'success' },
  { scenario: 'ACTION: show_stats again leaves it shown', toolName: 'control_editor', arguments: { action: 'show_stats', stat: 'fps' }, expected: 'success', assertions: [{ path: 'structuredContent.result.alreadyShown.0', equals: 'fps', label: 'a second show does not toggle the stat off' }] },
  { scenario: 'ACTION: hide_stats', toolName: 'control_editor', arguments: { action: 'hide_stats', stat: 'fps' }, expected: 'success' },
  { scenario: 'CONFIG: set_editor_mode', toolName: 'control_editor', arguments: { action: 'set_editor_mode', mode: 'EM_Default' }, expected: 'success' },
  { scenario: 'ERROR: set_editor_mode unknown mode', toolName: 'control_editor', arguments: { action: 'set_editor_mode', mode: 'NoSuchMode' }, expected: 'error|MODE_NOT_ACTIVATED' },
  { scenario: 'CONFIG: set_immersive_mode', toolName: 'control_editor', arguments: { action: 'set_immersive_mode', enabled: false }, expected: 'success' },
  { scenario: 'CONFIG: set_game_view', toolName: 'control_editor', arguments: { action: 'set_game_view', enabled: false }, expected: 'success' },
  { scenario: 'ACTION: undo', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'ACTION: redo', toolName: 'control_editor', arguments: { action: 'redo' }, expected: 'success' },
  { scenario: 'ERROR: restore point rollback with none open', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'rollback' }, expected: 'error|NO_RESTORE_POINT' },
  { scenario: 'ACTION: restore point begin', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'begin', label: 'integration' }, expected: 'success' },
  { scenario: 'ERROR: a second restore point while one is open', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'begin' }, expected: 'error|RESTORE_POINT_OPEN' },
  { scenario: 'ACTION: restore point status names it', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'status' }, expected: 'success', assertions: [{ path: 'structuredContent.result.label', equals: 'integration', label: 'the open restore point is the one begun' }] },
  { scenario: 'ACTION: restore point rollback with nothing since', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'rollback' }, expected: 'success' },
  { scenario: 'ACTION: restore point end', toolName: 'control_editor', arguments: { action: 'restore_point', op: 'end' }, expected: 'success' },
  { scenario: 'ACTION: save_all', toolName: 'control_editor', arguments: { action: 'save_all' }, expected: 'success' },
  // A listed path that is not dirty saves nothing and still succeeds.
  { scenario: 'ACTION: save_all assetPaths', toolName: 'control_editor', arguments: { action: 'save_all', assetPaths: ['/Game/NoSuchAsset'] }, expected: 'success' },

  // === EDITOR UI === the editor's own Slate UI read as an outline, and used through harmless input only (a hover, an
  // Escape key) on the main window; refusals for a ref no read handed out and for check on what is no check box.
  { scenario: 'INFO: read_editor_ui lists the windows and an outline', toolName: 'control_editor', arguments: { action: 'read_editor_ui', maxWidgets: 20 }, expected: 'success', captureResult: { key: 'mainWindowRef', fromField: 'result.windows.0.ref' } },
  { scenario: 'INFO: read_editor_ui of one window, words filtered', toolName: 'control_editor', arguments: { action: 'read_editor_ui', window: '0', text: 'Content', depth: 30, maxWidgets: 40 }, expected: 'success' },
  { scenario: 'INFO: read_editor_ui of one branch', toolName: 'control_editor', arguments: { action: 'read_editor_ui', from: '${captured:mainWindowRef}', depth: 2 }, expected: 'success' },
  { scenario: 'ACTION: use_editor_ui hovers the main window', toolName: 'control_editor', arguments: { action: 'use_editor_ui', ref: '${captured:mainWindowRef}', do: 'hover' }, expected: 'success' },
  { scenario: 'ACTION: use_editor_ui presses Escape on the main window', toolName: 'control_editor', arguments: { action: 'use_editor_ui', ref: '${captured:mainWindowRef}', do: 'press_key', key: 'Escape' }, expected: 'success' },
  { scenario: 'ERROR: use_editor_ui with a ref no read handed out', toolName: 'control_editor', arguments: { action: 'use_editor_ui', ref: 'e99999999', do: 'type', text: 'x', submit: false }, expected: 'error|WIDGET_NOT_FOUND' },
  { scenario: 'ERROR: use_editor_ui check on what is no check box', toolName: 'control_editor', arguments: { action: 'use_editor_ui', ref: '${captured:mainWindowRef}', do: 'check' }, expected: 'error|INVALID_ARGUMENT' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [FOCUS_ACTOR, PIE_PAWN] }, expected: 'success|not found' },
  // open_level refuses to drop unsaved level changes (DIRTY_PACKAGES), and the delete above made some.
  { scenario: 'Setup: save the cleanup before leaving the level', toolName: 'control_editor', arguments: { action: 'save_all' }, expected: 'success' },
  { scenario: 'ACTION: open_level', toolName: 'control_editor', arguments: { action: 'open_level', levelPath: '/Game/MCPTest/MainLevel' }, expected: 'success' },
];

runToolTests('control-editor', testCases, { folder: TEST_FOLDER });
