/**
 * system_control capability record catalog.
 *
 * Exactly 52 canonical CapabilityRecord entries mapped 1:1 to the 52
 * system_control actions in system-control-tool.ts (33 explicit enum actions
 * plus the 19 PERFORMANCE_ACTIONS spread into the enum). Each record is
 * grounded in the TypeScript handler map, the orchestrator routing in
 * consolidated-handler-registration.ts, command/path security utilities, and
 * the native HandleSystemControlAction accept list.
 *
 * Record order is the authored family-file concatenation below; this module
 * does not re-derive an action order.
 *
 * Families (10):
 * - console (8): show_fps, profile, set_quality, execute_command,
 *   console_command, set_cvar, set_resolution, set_fullscreen
 * - performance (19): PERFORMANCE_ACTIONS
 * - build (2): run_ubt, run_tests
 * - insights (10): trace session lifecycle + snapshot/analyze
 * - logs (3): subscribe, unsubscribe, spawn_category
 * - python (1): execute_python
 * - project (3): get_project_settings, set_project_setting, validate_assets
 * - widget (3): create_widget, show_widget, add_widget_child
 * - audio (1): play_sound
 * - viewport (1): screenshot
 * - render (1): lumen_update_scene
 *
 * Total: 8 + 19 + 2 + 10 + 3 + 1 + 3 + 3 + 1 + 1 + 1 = 52
 *
 * Routing: 50 actions use local TS dispatch (dispatchMode 'local') to a
 * specific bridge action; 2 (set_project_setting, execute_python) use the
 * fallback tool dispatch to system_control.
 */
import { type CapabilityRecord, type CapabilityRecordSource, createCapabilityRecord } from '../../index.js';

import { CONSOLE_RECORDS } from './console.js';
import { INSIGHTS_RECORDS } from './insights.js';
import { PERFORMANCE_A_RECORDS } from './performance-a.js';
import { PERFORMANCE_B_RECORDS } from './performance-b.js';
import { SYSTEM_OPS_RECORDS } from './system-ops.js';
import { WIDGET_AUDIO_VIEWPORT_RECORDS } from './widget-audio-viewport.js';

const SOURCES: readonly CapabilityRecordSource[] = [
  ...CONSOLE_RECORDS,
  ...PERFORMANCE_A_RECORDS,
  ...PERFORMANCE_B_RECORDS,
  ...SYSTEM_OPS_RECORDS,
  ...INSIGHTS_RECORDS,
  ...WIDGET_AUDIO_VIEWPORT_RECORDS,
];

export const SYSTEM_CONTROL_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const SYSTEM_CONTROL_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const SYSTEM_CONTROL_RECORD_COUNT = SYSTEM_CONTROL_RECORDS.length;
