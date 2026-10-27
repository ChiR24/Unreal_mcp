import { compareById } from '../../../../../utils/serialization/ordering.js';
import { type CapabilityRecordSource } from '../../model.js';
import { NETWORKING_FRAMEWORK_RECORDS } from './framework.data.js';
import { NETWORKING_INPUT_RECORDS } from './input.data.js';
import { NETWORKING_REPLICATION_RECORDS } from './replication.data.js';
import { NETWORKING_SESSION_RECORDS } from './session.data.js';
import { MANAGE_NETWORKING_FOLDS } from '../folds/manage-networking.folds.js';
import { applyFolds } from '../shared/fold.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_NETWORKING_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...NETWORKING_REPLICATION_RECORDS,
  ...NETWORKING_SESSION_RECORDS,
  ...NETWORKING_FRAMEWORK_RECORDS,
  ...NETWORKING_INPUT_RECORDS,
].sort(compareById);

export const MANAGE_NETWORKING_SOURCES: readonly CapabilityRecordSource[] = Object.freeze(applyFolds(MANAGE_NETWORKING_UNFOLDED_SOURCES, MANAGE_NETWORKING_FOLDS, 'manage_networking'));

