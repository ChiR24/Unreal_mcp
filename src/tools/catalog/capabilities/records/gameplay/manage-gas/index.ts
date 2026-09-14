import type { CapabilityRecordSource } from '../../../index.js';
import { MANAGE_GAS_FOLDS } from '../../folds/manage-gas.folds.js';
import { applyFolds } from '../../shared/fold.js';
import { GAS_RECORDS } from './gas.data.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_GAS_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = GAS_RECORDS;

export const MANAGE_GAS_SOURCES: readonly CapabilityRecordSource[] = applyFolds(GAS_RECORDS, MANAGE_GAS_FOLDS, 'manage_gas');
