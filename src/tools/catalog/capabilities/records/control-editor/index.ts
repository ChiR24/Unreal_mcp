/**
 * control_editor capability record catalog.
 *
 * 46 authored CapabilityRecordSource entries, folded by CONTROL_EDITOR_FOLDS
 * into the 21 shipped records that carry 48 callable legacy {tool, action}
 * pairs. Each record is grounded in the TypeScript handler bodies, native C++
 * ControlEditor domain dispatch, and the normalization inventory.
 *
 * Authored families (11 files, 46 records):
 * - session (8): PIE lifecycle play/stop/stop_pie/pause/resume/eject/possess,
 *   plus restart_editor (the editor process itself, not a PIE session)
 * - timing (4): game speed, fixed delta, frame stepping + alias
 * - recording (2): demo recording start/stop
 * - camera (6): view target, camera position/fov + aliases
 * - viewport (8): view mode, resolution, realtime, editor/immersive/game view, stats
 * - command (6): reflected api describe/invoke, open editor tab, console command,
 *   execute command, preferences
 * - screenshot (2): screenshot capture + alias
 * - bookmark (2): viewport bookmark create/jump
 * - asset (5): open/close asset, open level, focus actor, save all
 * - input (1): simulate input
 * - history (2): undo/redo
 *
 * 8 + 4 + 2 + 6 + 8 + 6 + 2 + 2 + 5 + 1 + 2 = 46. The counts are pinned by
 * control-editor-records.test.ts; do not restate them anywhere else.
 *
 * Record order is the authored family-file concatenation below, which reproduces
 * the canonical control_editor action enum exactly; this module does not
 * re-derive or rank an action order. The order test compares these records
 * against that enum directly, keeping the enum the only ordering authority.
 */
import {
  type CapabilityRecord,
  type CapabilityRecordSource,
  createCapabilityRecord,
} from '../../index.js';
import { ASSET_RECORDS } from './asset.js';
import { BOOKMARK_RECORDS } from './bookmark.js';
import { CAMERA_RECORDS } from './camera.js';
import { COMMAND_RECORDS } from './command.js';
import { HISTORY_RECORDS } from './history.js';
import { INPUT_RECORDS } from './input.js';
import { RECORDING_RECORDS } from './recording.js';
import { SCREENSHOT_RECORDS } from './screenshot.js';
import { SESSION_RECORDS } from './session.js';
import { TIMING_RECORDS } from './timing.js';
import { VIEWPORT_RECORDS } from './viewport.js';
import { applyFolds } from '../shared/fold.js';
import { CONTROL_EDITOR_FOLDS } from '../folds/control-editor.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const CONTROL_EDITOR_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...SESSION_RECORDS,
  ...TIMING_RECORDS,
  ...RECORDING_RECORDS,
  ...CAMERA_RECORDS,
  ...VIEWPORT_RECORDS,
  ...COMMAND_RECORDS,
  ...SCREENSHOT_RECORDS,
  ...BOOKMARK_RECORDS,
  ...ASSET_RECORDS,
  ...INPUT_RECORDS,
  ...HISTORY_RECORDS,
];

const SOURCES: readonly CapabilityRecordSource[] = applyFolds(CONTROL_EDITOR_UNFOLDED_SOURCES, CONTROL_EDITOR_FOLDS, 'control_editor');

export const CONTROL_EDITOR_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const CONTROL_EDITOR_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const CONTROL_EDITOR_RECORD_COUNT = CONTROL_EDITOR_RECORDS.length;
