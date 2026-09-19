/**
 * system_control capability record catalog.
 *
 * 57 authored CapabilityRecordSource entries covering the 57 system_control
 * actions in system-control-tool.ts (38 explicit enum actions plus the 19
 * PERFORMANCE_ACTIONS spread into the enum), folded by SYSTEM_CONTROL_FOLDS
 * into the 21 shipped records (SYSTEM_CONTROL_FOLDED_RECORD_COUNT in
 * system-control-test-helpers.ts). Each record is
 * grounded in the TypeScript handler map, the orchestrator routing in
 * consolidated-handler-registration.ts, command/path security utilities, and
 * the native HandleSystemControlAction accept list.
 *
 * Record order is the authored family-file concatenation below; this module
 * does not re-derive an action order.
 *
 * Authored families (12):
 * - console (8): show_fps, profile, set_quality, execute_command,
 *   console_command, set_cvar, set_resolution, set_fullscreen
 * - performance (19): PERFORMANCE_ACTIONS
 * - build (4): run_ubt, run_tests, package_project, package_status
 * - insights (10): trace session lifecycle + snapshot/analyze
 * - logs (3): subscribe, unsubscribe, spawn_category
 * - python (1): execute_python
 * - project (3): get_project_settings, set_project_setting, validate_assets
 * - plugin (3): list_plugins, enable_plugin, disable_plugin
 * - widget (3): create_widget, show_widget, add_widget_child
 * - audio (1): play_sound
 * - viewport (1): screenshot
 * - render (1): lumen_update_scene
 *
 * Total: 8 + 19 + 4 + 10 + 3 + 1 + 3 + 3 + 3 + 1 + 1 + 1 = 57
 * (by source file: console 8, insights 10, performance-a 10 + performance-b 9,
 * plugins 3, system-ops 12, widget-audio-viewport 5.)
 *
 * Routing: 50 actions use local TS dispatch (dispatchMode 'local') to a
 * specific bridge action; 5 (set_project_setting, execute_python,
 * list_plugins, enable_plugin, disable_plugin) use the fallback tool dispatch
 * to system_control.
 */
import { type CapabilityRecord, type CapabilityRecordSource, createCapabilityRecord } from '../../index.js';

import { CONSOLE_RECORDS } from './console.js';
import { INSIGHTS_RECORDS } from './insights.js';
import { PERFORMANCE_A_RECORDS } from './performance-a.js';
import { PERFORMANCE_B_RECORDS } from './performance-b.js';
import { PLUGIN_RECORDS } from './plugins.js';
import { SYSTEM_OPS_RECORDS } from './system-ops.js';
import { WIDGET_AUDIO_VIEWPORT_RECORDS } from './widget-audio-viewport.js';
import { applyFolds } from '../shared/fold.js';
import { SYSTEM_CONTROL_FOLDS } from '../folds/system-control.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const SYSTEM_CONTROL_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...CONSOLE_RECORDS,
  ...PERFORMANCE_A_RECORDS,
  ...PERFORMANCE_B_RECORDS,
  ...SYSTEM_OPS_RECORDS,
  ...PLUGIN_RECORDS,
  ...INSIGHTS_RECORDS,
  ...WIDGET_AUDIO_VIEWPORT_RECORDS,
];

const SOURCES: readonly CapabilityRecordSource[] = applyFolds(SYSTEM_CONTROL_UNFOLDED_SOURCES, SYSTEM_CONTROL_FOLDS, 'system_control');

export const SYSTEM_CONTROL_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const SYSTEM_CONTROL_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const SYSTEM_CONTROL_RECORD_COUNT = SYSTEM_CONTROL_RECORDS.length;
