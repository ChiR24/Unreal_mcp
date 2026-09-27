/**
 * system_control capability record catalog.
 */
import { type CapabilityRecordSource } from '../../model.js';

import { CONSOLE_RECORDS } from './console.js';
import { INSIGHTS_RECORDS } from './insights.js';
import { PERFORMANCE_RECORDS } from './performance.js';
import { PLUGIN_RECORDS } from './plugins.js';
import { SYSTEM_OPS_RECORDS } from './system-ops.js';
import { WIDGET_AUDIO_VIEWPORT_RECORDS } from './widget-audio-viewport.js';
import { applyFolds } from '../shared/fold.js';
import { SYSTEM_CONTROL_FOLDS } from '../folds/system-control.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const SYSTEM_CONTROL_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...CONSOLE_RECORDS,
  ...PERFORMANCE_RECORDS,  ...SYSTEM_OPS_RECORDS,
  ...PLUGIN_RECORDS,
  ...INSIGHTS_RECORDS,
  ...WIDGET_AUDIO_VIEWPORT_RECORDS,
];

export const SYSTEM_CONTROL_SOURCES: readonly CapabilityRecordSource[] = applyFolds(SYSTEM_CONTROL_UNFOLDED_SOURCES, SYSTEM_CONTROL_FOLDS, 'system_control');

