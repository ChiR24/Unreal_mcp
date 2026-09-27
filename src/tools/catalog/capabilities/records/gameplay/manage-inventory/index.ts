import type { CapabilityRecordSource } from '../../../model.js';
import { INVENTORY_RECORDS } from './inventory.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_INVENTORY_FOLDS } from '../../folds/manage-inventory.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_INVENTORY_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...INVENTORY_RECORDS,];

export const MANAGE_INVENTORY_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_INVENTORY_UNFOLDED_SOURCES, MANAGE_INVENTORY_FOLDS, 'manage_inventory');
