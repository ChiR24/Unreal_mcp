/**
 * Global inspection records (7 actions): project/world/editor settings,
 * viewport info, editor selection.
 *
 * get_project_settings is the primary canonical occurrence of
 * cap:shared:get_project_settings (class A, shared with system_control).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const D = 'inspect';

export const GLOBAL_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_project_settings', dispatchAction: 'get_project_settings', domain: D, family: 'global',
    topics: ['project settings', 'project config', 'engine version', 'project name', 'game settings'],
    summary: 'Return project settings key/value pairs.',
    whenToUse: ['Project settings must be inspected.'],
    whenNotToUse: ['Editor settings are needed; use get_editor_settings.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_project_settings' },
    exampleOutput: {
      success: true, message: 'Project settings', projectName: 'Demo',
      engineVersion: '5.6.0-0+++UE5', buildConfig: 'Development',
      projectDir: '../../../Demo/', projectVersion: '1.0.0.0', companyName: 'Acme',
    },
    // Every field the handler produces must be declared or output projection
    // discards it and the capability answers with {success, message} only.
    outputProps: {
      projectName: { type: 'string', description: 'Project name.' },
      engineVersion: { type: 'string', description: 'Unreal Engine version string.' },
      buildConfig: { type: 'string', description: 'Build configuration, e.g. Development.' },
      projectDir: { type: 'string', description: 'Project directory on disk.' },
      description: { type: 'string', description: 'Project description.' },
      homepage: { type: 'string', description: 'Project homepage URL.' },
      supportContact: { type: 'string', description: 'Project support contact.' },
      projectVersion: { type: 'string', description: 'Project version string.' },
      companyName: { type: 'string', description: 'Company name.' },
      copyrightNotice: { type: 'string', description: 'Copyright notice.' },
      projectID: { type: 'string', description: 'Project GUID.' },
      startInVR: { type: 'boolean', description: 'Whether the project starts in VR.' },
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_world_settings', dispatchAction: 'get_world_settings', domain: D, family: 'global',
    summary: 'Return the current world/level settings summary (worldName, gravity, killZ, time).',
    whenToUse: ['The current level\'s world settings must be inspected.'],
    whenNotToUse: ['Project-wide settings are needed; use get_project_settings.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_world_settings' },
    exampleOutput: {
      success: true, message: 'World settings', worldName: 'Demo', levelName: 'Demo',
      packageName: '/Game/Maps/Demo', timeSeconds: 12.5, realTimeSeconds: 30.2,
      deltaTimeSeconds: 0.0166, hasBegunPlay: false, isPlayInEditor: false,
      killZ: -1048575, worldGravityZ: -980, timeDilation: 1, enableWorldBoundsChecks: true,
      defaultGameMode: '/Game/Blueprints/BP_GameMode.BP_GameMode_C',
    },
    // Declared or the handler's payload is projected away, leaving worldName alone
    // while the summary promises gravity, killZ and time.
    outputProps: {
      worldName: { type: 'string', description: 'Current world name.' },
      levelName: { type: 'string', description: 'Current level name.' },
      packageName: { type: 'string', description: 'Package name of the world asset.' },
      timeSeconds: { type: 'number', description: 'World time in seconds.' },
      realTimeSeconds: { type: 'number', description: 'Real (unpaused) time in seconds.' },
      deltaTimeSeconds: { type: 'number', description: 'Last frame delta in seconds.' },
      hasBegunPlay: { type: 'boolean', description: 'Whether the world has begun play.' },
      isPlayInEditor: { type: 'boolean', description: 'Whether this world is a PIE world.' },
      isPaused: { type: 'boolean', description: 'Whether game time stands still: control_editor pause, or the game paused itself (a title screen or a pause menu). Nothing moves or simulates until control_editor resume (unpauseGame for a pause the game holds).' },
      killZ: { type: 'number', description: 'Z height below which actors are killed.' },
      worldGravityZ: { type: 'number', description: 'World gravity along Z.' },
      timeDilation: { type: 'number', description: 'Global time dilation multiplier.' },
      enableWorldBoundsChecks: { type: 'boolean', description: 'Whether world bounds checks are enabled.' },
      defaultGameMode: { type: 'string', description: 'Default GameMode class path set on WorldSettings.' },
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_viewport_info', dispatchAction: 'get_viewport_info', domain: D, family: 'global',
    summary: 'Return viewport information (view target, camera manager, world type).',
    whenToUse: ['Viewport and camera state must be inspected.'],
    whenNotToUse: ['A screenshot is needed; use control_editor.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_viewport_info' },
    exampleOutput: { success: true, message: 'Viewport info', width: 1920, height: 1080 },
    // Declared so the handler's viewport size survives output projection.
    outputProps: {
      width: { type: 'number', description: 'Active viewport width in pixels.' },
      height: { type: 'number', description: 'Active viewport height in pixels.' },
      // Every get_editor_state variant sends it (the five reads share one prelude), so it is declared once for the family.
      shadersCompiling: { type: 'number', description: 'Every get_editor_state variant: shader compile jobs still outstanding. Above 0 the viewport, and a screenshot of it, shows the engine default material (black foliage, grey ground) for what they cover; a warning says so.' },
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_selected_actors', dispatchAction: 'get_selected_actors', domain: D, family: 'global',
    topics: ['selected actors', 'selection', 'what is selected', 'current selection', 'editor selection'],
    summary: 'Return the actors currently selected in the editor viewport.',
    whenToUse: ['The current editor selection must be inspected.'],
    whenNotToUse: ['All actors are needed; use list_objects.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_selected_actors' },
    exampleOutput: { success: true, message: 'Selected actors', actors: [] },
    outputProps: { actors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Selected actor info objects.' } },
  }),
  // A model asked to fix "this node" or "what I have open" could not tell what that was.
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_editor_focus', dispatchAction: 'get_editor_focus', domain: D, family: 'global',
    topics: ['what is open', 'what the user is looking at', 'open asset editors', 'current blueprint', 'selected nodes', 'focused graph', 'this node', 'recent errors', 'editor focus'],
    summary: 'Read what the user is working on: the open asset editors (most recently used first), each open Blueprint editor\'s graph in front and its selected nodes (title and nodeId), the selected actors, the distinct warnings and errors of the last minute, and the Blueprint runtime errors of the last play session.',
    whenToUse: ['A request names something the user has in front of them ("this node", "the Blueprint I have open", "fix that error") and it must be resolved to an asset, a graph or a node.', 'Work should start from the editor state the user left (the selected nodes, the open editors).'],
    whenNotToUse: ['Only the selected actors are needed (use editorState selected_actors).', 'The whole log is wanted (use the log read).'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_editor_focus' },
    exampleOutput: {
      success: true, message: '2 asset editor(s) open; BP_Door has 1 node selected in EventGraph.',
      openEditors: [{ asset: '/Game/Blueprints/BP_Door', editor: 'BlueprintEditor' }, { asset: '/Game/Materials/M_Wood', editor: 'MaterialEditor' }],
      blueprints: [{ blueprint: '/Game/Blueprints/BP_Door', graph: 'EventGraph', selectedNodes: [{ title: 'Set Actor Location', nodeId: '1C2B7F7A4B4E1F0D9E1A2B3C4D5E6F70', class: 'K2Node_CallFunction' }] }],
      selectedActors: ['Door_01'], recentWarnings: [], scriptErrors: [],
    },
    outputProps: {
      openEditors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each asset open in an editor, most recently used first: asset (its path) and editor (BlueprintEditor, MaterialEditor, ...).' },
      blueprints: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each open Blueprint editor, most recently used first: blueprint (path), graph (the graph whose editor last had keyboard focus; absent until one has) and selectedNodes [{title, nodeId, class}]; nodeId is what the graph tools take.' },
      selectedActors: { type: 'array', items: { type: 'string' }, description: 'Labels of the actors selected in the level (at most 50).' },
      recentWarnings: { type: 'array', items: { type: 'string' }, description: 'The distinct warnings and errors logged in the last minute ("3x Category: message"), at most 10, a Blueprint runtime error first with the node that raised it.' },
      scriptErrors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'The Blueprint runtime errors of the last play session, as runtime_report lists them (message, node, nodeId, graph, blueprintPath, count).' },
    },
  }),
];

export const EDITOR_SETTINGS_RECORD: CapabilityRecordSource = buildCoreRecord({
  parentTool: 'inspect', action: 'get_editor_settings', dispatchAction: 'get_editor_settings', domain: D, family: 'global',
  summary: 'Return editor settings key/value pairs.',
  whenToUse: ['Editor settings must be inspected.'],
  whenNotToUse: ['Project settings are needed; use get_project_settings.'],
  inputProps: {},
  required: [],
  effect: 'read',
  exampleInput: { action: 'get_editor_settings' },
  exampleOutput: {
    success: true, message: 'Editor settings', mouseSensitivity: 1.0, mouseScrollCameraSpeed: 1.0,
    useDistanceScaledCamera: false, isSimulating: false, isPIEActive: false,
    gameAgnosticSavedFPS: 60, isEditor: true, gRunningCommandlet: 0,
  },
  outputProps: {
    mouseSensitivity: { type: 'number', description: 'Viewport mouse sensitivity.' },
    mouseScrollCameraSpeed: { type: 'number', description: 'Mouse scroll camera speed.' },
    useDistanceScaledCamera: { type: 'boolean', description: 'Whether distance-scaled camera speed is used.' },
    isSimulating: { type: 'boolean', description: 'Whether the editor is simulating.' },
    isPIEActive: { type: 'boolean', description: 'Whether a PIE session is active.' },
    gameAgnosticSavedFPS: { type: 'number', description: 'Saved game-agnostic max FPS.' },
    isEditor: { type: 'boolean', description: 'Whether running in the editor.' },
    gRunningCommandlet: { type: 'number', description: '1 when running as a commandlet, else 0.' },
  },
});
