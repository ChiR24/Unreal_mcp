/**
 * control_actor capability record catalog.
 */
import type { CapabilityRecordSource } from '../../model.js';

import { ADVANCED_RECORDS } from './advanced-records.js';
import { COMPONENT_RECORDS } from './component-records.js';
import { SEARCH_RECORDS } from './search-records.js';
import { SPAWN_RECORDS } from './spawn-records.js';
import { STATE_RECORDS } from './state-records.js';
import { TRANSFORM_RECORDS } from './transform-records.js';
import { applyFolds } from '../shared/fold.js';
import { CONTROL_ACTOR_FOLDS } from '../folds/control-actor.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const CONTROL_ACTOR_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...SPAWN_RECORDS,
  ...TRANSFORM_RECORDS,
  ...COMPONENT_RECORDS,
  ...STATE_RECORDS,
  ...SEARCH_RECORDS,
  ...ADVANCED_RECORDS,
];

export const CONTROL_ACTOR_SOURCES: readonly CapabilityRecordSource[] = applyFolds(CONTROL_ACTOR_UNFOLDED_SOURCES, CONTROL_ACTOR_FOLDS, 'control_actor');

