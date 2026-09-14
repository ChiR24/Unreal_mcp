/**
 * Global inspection records (7 actions): project/world/editor settings,
 * viewport info, editor selection.
 *
 * get_project_settings is the primary canonical occurrence of
 * cap:shared:get_project_settings (class A, shared with system_control).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';

const D = 'inspect';
const NR = 'Distinct inspect verb and target; no cross-tool duplicate.';

export const GLOBAL_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_project_settings', dispatchAction: 'get_project_settings', domain: D, family: 'global',
    topics: ['project settings', 'project config', 'engine version', 'project name', 'game settings'],
    summary: 'Return project settings key/value pairs.',
    whenToUse: ['Project settings must be inspected.'],
    whenNotToUse: ['Editor settings are needed; use get_editor_settings.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
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
    normalizationClass: 'A_TRUE_DUPLICATE',
    normalizationRationale: 'True duplicate (cap:shared:get_project_settings) shared with system_control; inspect is the primary canonical occurrence per the normalization inventory (class A, keep). Implemented in both TS (inspect-global-actions.ts GLOBAL_INSPECT_ACTIONS) and native (bIsGlobalAction).',
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_world_settings', dispatchAction: 'get_world_settings', domain: D, family: 'global',
    summary: 'Return the current world/level settings summary (worldName, gravity, killZ, time).',
    whenToUse: ['The current level\'s world settings must be inspected.'],
    whenNotToUse: ['Project-wide settings are needed; use get_project_settings.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
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
      killZ: { type: 'number', description: 'Z height below which actors are killed.' },
      worldGravityZ: { type: 'number', description: 'World gravity along Z.' },
      timeDilation: { type: 'number', description: 'Global time dilation multiplier.' },
      enableWorldBoundsChecks: { type: 'boolean', description: 'Whether world bounds checks are enabled.' },
      defaultGameMode: { type: 'string', description: 'Default GameMode class path set on WorldSettings.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_viewport_info', dispatchAction: 'get_viewport_info', domain: D, family: 'global',
    summary: 'Return viewport information (view target, camera manager, world type).',
    whenToUse: ['Viewport and camera state must be inspected.'],
    whenNotToUse: ['A screenshot is needed; use control_editor.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_viewport_info' },
    exampleOutput: { success: true, message: 'Viewport info', width: 1920, height: 1080 },
    // Declared so the handler's viewport size survives output projection.
    outputProps: {
      width: { type: 'number', description: 'Active viewport width in pixels.' },
      height: { type: 'number', description: 'Active viewport height in pixels.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_selected_actors', dispatchAction: 'get_selected_actors', domain: D, family: 'global',
    topics: ['selected actors', 'selection', 'what is selected', 'current selection', 'editor selection'],
    summary: 'Return the actors currently selected in the editor viewport.',
    whenToUse: ['The current editor selection must be inspected.'],
    whenNotToUse: ['All actors are needed; use list_objects.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_selected_actors' },
    exampleOutput: { success: true, message: 'Selected actors', actors: [] },
    outputProps: { actors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Selected actor info objects.' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];

export const EDITOR_SETTINGS_RECORD: CapabilityRecordSource = buildCoreRecord({
  parentTool: 'inspect', action: 'get_editor_settings', dispatchAction: 'get_editor_settings', domain: D, family: 'global',
  summary: 'Return editor settings key/value pairs.',
  whenToUse: ['Editor settings must be inspected.'],
  whenNotToUse: ['Project settings are needed; use get_project_settings.'],
  inputProps: {},
  required: [],
  effect: 'read', costLatency: 'instant', costResources: 'low',
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
  normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
});
