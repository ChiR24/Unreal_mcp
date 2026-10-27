import type { CapabilityRecordSource } from '../../../model.js';
import { MANAGE_GAS_FOLDS } from '../../folds/manage-gas.folds.js';
import { applyFolds } from '../../shared/fold.js';
import { GAS_RECORDS } from './gas.data.js';

export const MANAGE_GAS_SOURCES: readonly CapabilityRecordSource[] = applyFolds(GAS_RECORDS, MANAGE_GAS_FOLDS, 'manage_gas');
