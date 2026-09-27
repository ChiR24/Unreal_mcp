/**
 * inspect capability record catalog.
 */
import { type CapabilityRecordSource } from '../../model.js';

import { COMPONENT_ACTOR_RECORDS } from './component-actor.data.js';
import { EDITOR_SETTINGS_RECORD, GLOBAL_RECORDS } from './global.data.js';
import { RUNTIME_RECORDS } from './runtime.data.js';
import { STATS_RECORDS } from './stats.data.js';
import { OBJECT_PROPERTY_RECORDS } from './object-property.data.js';
import { applyFolds } from '../shared/fold.js';
import { INSPECT_FOLDS } from '../folds/inspect.folds.js';

/**
 * Record order is the authored data-file concatenation; this module does not
 * re-derive an action order.
 */
/** The authored records before folding; per-action contract tests pin these. */
export const INSPECT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...OBJECT_PROPERTY_RECORDS,
  ...COMPONENT_ACTOR_RECORDS,
  ...RUNTIME_RECORDS,
  ...GLOBAL_RECORDS,
  ...STATS_RECORDS,
  EDITOR_SETTINGS_RECORD,
];

export const INSPECT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(INSPECT_UNFOLDED_SOURCES, INSPECT_FOLDS, 'inspect');

