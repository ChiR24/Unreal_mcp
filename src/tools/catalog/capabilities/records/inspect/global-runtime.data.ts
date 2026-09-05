/**
 * Global and runtime inspection records (10 actions).
 *
 * Grounded in:
 * - src/tools/handlers/inspect/inspect-global-actions.ts: GLOBAL_INSPECT_ACTIONS
 *   (get_project_settings, get_editor_settings, get_performance_stats,
 *   get_memory_stats, get_scene_stats, get_viewport_info, get_selected_actors)
 *   dispatch to the inspect bridge route; runtime_report and pie_report
 *   dispatch to inspect with their original action; inspect_class/inspect_cdo
 *   have dedicated handlers (covered in object-property.data.ts).
 * - inspect-actions.ts: pie_report aliases to runtime_report for switch
 *   routing, but handleRuntimeReport re-dispatches the original pie_report
 *   action, so the record dispatches pie_report.
 * - normalization-inventory.json: catalogs all 36 inspect actions.
 *   get_project_settings is the primary canonical occurrence of
 *   cap:shared:get_project_settings (class A, shared with system_control);
 *   the other 35 are class C distinct targets.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const D = 'inspect';
const NR = 'Distinct inspect verb and target; no cross-tool duplicate.';

// runtime_report and pie_report share one handler
// (McpAutomationBridge_EnvironmentHandlersInspectRuntime.cpp) which emits every
// field below. Neither record declared ANY of them, and the gateway projects a
// result to schema-declared names only, so both answered "Runtime inspection
// report generated" carrying nothing — with the two capabilities returning the
// byte-identical empty payload that made them look like duplicates.
const RUNTIME_REPORT_OUTPUT = {
  worldName: { type: 'string', description: 'Name of the world the report describes.' },
  worldPath: { type: 'string', description: 'Package path of that world.' },
  worldType: { type: 'string', description: 'World type, e.g. PIE or Editor.' },
  isPIE: { type: 'boolean', description: 'Whether a PIE session is active.' },
  actors: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Matching runtime actors and their inspected components/properties.' },
  count: { type: 'number', description: 'Number of actors returned after filtering.' },
  totalActorCount: { type: 'number', description: 'Total actors in the inspected world.' },
  // These four are McpDescribeRuntimeActor() results — full actor descriptions,
  // not path strings. Declared as `string` they did worse than the missing
  // fields above: the output validator rejected the handler's real payload, so
  // every pie_report during an actual PIE session failed OUTPUT_SCHEMA_VIOLATION
  // and returned nothing at all. The capability only "worked" when no player
  // controller existed, i.e. when there was nothing to report.
  // Dogfood #139: the plugin emits these three as object-path strings (pinned by BB-036); declaring
  // them as objects made every PIE report fail output validation.
  playerController: { type: 'string', description: 'Object path of the active PlayerController (inspect_object it for details).' },
  pawn: { type: 'string', description: 'Object path of the possessed pawn; inspect it to find where the player actually is.' },
  viewTarget: { type: 'string', description: 'Object path of the current view target.' },
  // Carries nested cameraLocation/cameraRotation objects. There are no
  // top-level camera fields — the handler only ever sets them inside here.
  playerCameraManager: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'PlayerCameraManager described as a runtime actor, plus cameraLocation and cameraRotation as {x,y,z} / {pitch,yaw,roll} objects.' },
} as const;

export const GLOBAL_RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'runtime_report', dispatchAction: 'runtime_report', domain: D, family: 'runtime',
    summary: 'Return a runtime report for the current PIE/simulate session.',
    whenToUse: ['Runtime state of actors/components/properties must be inspected during PIE.'],
    whenNotToUse: ['The editor is not in PIE; the report will be empty.'],
    inputProps: {
      filter: P.filter, actorName: P.actorName, name: P.name,
      componentName: P.componentName, componentNames: P.componentNames,
      propertyName: P.propertyName, propertyPath: P.propertyPath, propertyNames: P.propertyNames,
    },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    outputProps: { ...RUNTIME_REPORT_OUTPUT },
    outputRequired: [],
    exampleInput: { action: 'runtime_report', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Runtime report', worldName: 'Demo', worldType: 'PIE', isPIE: true, count: 1, totalActorCount: 39 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'pie_report', dispatchAction: 'pie_report', domain: D, family: 'runtime',
    summary: 'Return a PIE-specific runtime report (TS aliases to runtime_report for routing, then re-dispatches pie_report).',
    whenToUse: ['PIE-only runtime state must be inspected.'],
    whenNotToUse: ['A general runtime report is needed; use runtime_report.'],
    inputProps: {
      filter: P.filter, actorName: P.actorName, name: P.name,
      componentName: P.componentName, componentNames: P.componentNames,
      propertyName: P.propertyName, propertyPath: P.propertyPath, propertyNames: P.propertyNames,
    },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    outputProps: { ...RUNTIME_REPORT_OUTPUT },
    outputRequired: [],
    exampleInput: { action: 'pie_report' },
    exampleOutput: { success: true, message: 'PIE report', worldName: 'Demo', worldType: 'PIE', isPIE: true, count: 0, totalActorCount: 39 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'inspect-actions.ts aliases pie_report to runtime_report for switch routing, but inspect-global-actions.ts re-dispatches the original pie_report action; the record preserves the canonical pie_report dispatch rather than collapsing it into runtime_report.',
  }),
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
    // Every field below is already produced by the handler
    // (McpAutomationBridge_EnvironmentHandlersInspectSettings.cpp,
    // get_project_settings). Undeclared here, output projection discarded all
    // of it and the capability answered with {success, message} only.
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
    // The handler (McpAutomationBridge_EnvironmentHandlersInspectSettings.cpp,
    // get_world_settings) already emits every field below. They were absent
    // here, and the gateway projects a result to schema-declared names ONLY, so
    // the entire payload was discarded and callers received `worldName` alone —
    // while this record's own summary promised gravity, killZ and time.
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
    // Declared so the handler's viewport size survives output projection; with
    // no outputProps at all the response projected to {success, message} and
    // reported "Viewport info retrieved" carrying nothing.
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
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_scene_stats', dispatchAction: 'get_scene_stats', domain: D, family: 'stats',
    summary: 'Return scene statistics (actor counts, component counts, etc.).',
    whenToUse: ['Scene-level statistics must be inspected.'],
    whenNotToUse: ['Runtime performance is needed; use get_performance_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_scene_stats' },
    exampleOutput: { success: true, message: 'Scene stats', actorCount: 42 },
    outputProps: { actorCount: { type: 'number', description: 'Level-actor count — the SAME set control_actor.list reports.' }, totalWorldActors: { type: 'number', description: 'Raw world actor count including editor-internal actors (explains the gap vs actorCount).' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_performance_stats', dispatchAction: 'get_performance_stats', domain: D, family: 'stats',
    summary: 'Return performance statistics (frame rate, frame time, draw calls).',
    whenToUse: ['Performance metrics must be inspected.'],
    whenNotToUse: ['Scene composition is needed; use get_scene_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_performance_stats' },
    exampleOutput: {
      success: true, message: 'Performance stats', worldType: 'Editor',
      threadTimersAreProcessGlobal: true, deltaSeconds: 0.0166, frameTimeMs: 16.6,
      estimatedFps: 60, fps: 60, gameThreadMs: 4.2, renderThreadMs: 3.1, rhiThreadMs: 1.8,
      gpuMs: 5.5, actorCount: 42, isBenchmarking: false, useFixedTimeStep: false,
    },
    outputProps: {
      worldType: { type: 'string', description: 'World measured: "Editor", "PIE", or "None".' },
      threadTimersAreProcessGlobal: { type: 'boolean', description: 'Thread timers are process-global, not per-world.' },
      // `fps` is frame-delta derived and an idle editor throttles that delta
      // hard (observed: fps 3 while the viewport showed 60). These qualify it.
      busiestThreadMs: { type: 'number', description: 'Slowest of game/render/GPU thread times, in milliseconds.' },
      threadTimeDerivedFps: { type: 'number', description: 'FPS implied by busiestThreadMs; unaffected by editor idle throttling.' },
      frameDeltaMayBeEditorThrottled: { type: 'boolean', description: 'True outside PIE, where an idle editor throttles the frame delta and makes fps read far lower than actual.' },
      deltaSeconds: { type: 'number', description: 'Frame delta time in seconds.' },
      frameTimeMs: { type: 'number', description: 'Frame time in milliseconds.' },
      estimatedFps: { type: 'number', description: 'Estimated frames per second from delta time.' },
      fps: { type: 'number', description: 'Reported frames per second (same as estimatedFps).' },
      gameThreadMs: { type: 'number', description: 'Game thread time in milliseconds.' },
      renderThreadMs: { type: 'number', description: 'Render thread time in milliseconds.' },
      rhiThreadMs: { type: 'number', description: 'RHI thread time in milliseconds.' },
      gpuMs: { type: 'number', description: 'GPU frame time in milliseconds.' },
      actorCount: { type: 'number', description: 'Actor count of the measured world.' },
      isBenchmarking: { type: 'boolean', description: 'Whether the engine is in benchmarking mode.' },
      useFixedTimeStep: { type: 'boolean', description: 'Whether a fixed time step is active.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_memory_stats', dispatchAction: 'get_memory_stats', domain: D, family: 'stats',
    summary: 'Return memory statistics (allocated, virtual, resource counts).',
    whenToUse: ['Memory usage must be inspected.'],
    whenNotToUse: ['Performance timing is needed; use get_performance_stats.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_memory_stats' },
    exampleOutput: {
      success: true, message: 'Memory stats',
      totalPhysicalBytes: 17179869184, availablePhysicalBytes: 8589934592, usedPhysicalBytes: 8589934592,
      peakUsedPhysicalBytes: 10737418240, totalVirtualBytes: 140737488355328,
      availableVirtualBytes: 137438953472, usedVirtualBytes: 3298534883328, peakUsedVirtualBytes: 35184372088832,
      totalPhysicalMB: 16384, totalVirtualMB: 134217728, availablePhysicalMB: 8192, availableVirtualMB: 131072,
      usedPhysicalMB: 8192, usedVirtualMB: 3145728, peakUsedPhysicalMB: 10240, peakUsedVirtualMB: 33554432,
    },
    outputProps: {
      totalPhysicalBytes: { type: 'number', description: 'Total physical memory in bytes.' },
      availablePhysicalBytes: { type: 'number', description: 'Available physical memory in bytes.' },
      usedPhysicalBytes: { type: 'number', description: 'Used physical memory in bytes.' },
      peakUsedPhysicalBytes: { type: 'number', description: 'Peak used physical memory in bytes.' },
      totalVirtualBytes: { type: 'number', description: 'Total virtual memory in bytes.' },
      availableVirtualBytes: { type: 'number', description: 'Available virtual memory in bytes.' },
      usedVirtualBytes: { type: 'number', description: 'Used virtual memory in bytes.' },
      peakUsedVirtualBytes: { type: 'number', description: 'Peak used virtual memory in bytes.' },
      totalPhysicalMB: { type: 'number', description: 'Total physical memory in megabytes.' },
      totalVirtualMB: { type: 'number', description: 'Total virtual memory in megabytes.' },
      availablePhysicalMB: { type: 'number', description: 'Available physical memory in megabytes.' },
      availableVirtualMB: { type: 'number', description: 'Available virtual memory in megabytes.' },
      usedPhysicalMB: { type: 'number', description: 'Used physical memory in megabytes.' },
      usedVirtualMB: { type: 'number', description: 'Used virtual memory in megabytes.' },
      peakUsedPhysicalMB: { type: 'number', description: 'Peak used physical memory in megabytes.' },
      peakUsedVirtualMB: { type: 'number', description: 'Peak used virtual memory in megabytes.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
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
  }),
];
