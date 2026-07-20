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
    exampleInput: { action: 'runtime_report', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Runtime report' },
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
    exampleInput: { action: 'pie_report' },
    exampleOutput: { success: true, message: 'PIE report' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'inspect-actions.ts aliases pie_report to runtime_report for switch routing, but inspect-global-actions.ts re-dispatches the original pie_report action; the record preserves the canonical pie_report dispatch rather than collapsing it into runtime_report.',
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_project_settings', dispatchAction: 'get_project_settings', domain: D, family: 'global',
    summary: 'Return project settings key/value pairs.',
    whenToUse: ['Project settings must be inspected.'],
    whenNotToUse: ['Editor settings are needed; use get_editor_settings.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_project_settings' },
    exampleOutput: { success: true, message: 'Project settings' },
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
    exampleOutput: { success: true, message: 'World settings', worldName: 'Demo' },
    outputProps: { worldName: { type: 'string', description: 'Current world name.' } },
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
    exampleOutput: { success: true, message: 'Viewport info' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_selected_actors', dispatchAction: 'get_selected_actors', domain: D, family: 'global',
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
    exampleOutput: { success: true, message: 'Scene stats' },
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
    exampleOutput: { success: true, message: 'Performance stats' },
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
    exampleOutput: { success: true, message: 'Memory stats' },
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
    exampleOutput: { success: true, message: 'Editor settings' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
