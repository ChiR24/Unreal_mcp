/**
 * control_editor capability record catalog.
 *
 * The authored CapabilityRecordSource entries, folded by CONTROL_EDITOR_FOLDS
 * into the shipped records. Each record is grounded in the native C++
 * ControlEditor domain dispatch.
 *
 * Authored families:
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
 * - history: undo/redo and restore_point
 * - playtest (1): a pawn of the running game walked to a goal, the leg judged
 *
 * The record counts are pinned by aggregate.ts (ALL_CAPABILITY_RECORD_COUNT) and the
 * generated catalog, not here.
 *
 * Record order is the family-file concatenation below.
 */
import { type CapabilityRecordSource } from '../../model.js';
import { ASSET_RECORDS } from './asset.js';
import { BOOKMARK_RECORDS } from './bookmark.js';
import { CAMERA_RECORDS } from './camera.js';
import { COMMAND_RECORDS } from './command.js';
import { EXPLORE_RECORDS } from './explore.js';
import { HISTORY_RECORDS } from './history.js';
import { INPUT_RECORDS } from './input.js';
import { PLAYTEST_RECORDS } from './playtest.js';
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
  ...PLAYTEST_RECORDS,
  ...EXPLORE_RECORDS,
];

export const CONTROL_EDITOR_SOURCES: readonly CapabilityRecordSource[] = applyFolds(CONTROL_EDITOR_UNFOLDED_SOURCES, CONTROL_EDITOR_FOLDS, 'control_editor');

