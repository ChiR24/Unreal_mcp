import type { CapabilityRecordSource } from '../../../index.js';
import { INVENTORY_1 } from './inventory-1.data.js';
import { INVENTORY_2 } from './inventory-2.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_INVENTORY_FOLDS } from '../../folds/manage-inventory.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_INVENTORY_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...INVENTORY_1,
  ...INVENTORY_2,
];

export const MANAGE_INVENTORY_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_INVENTORY_UNFOLDED_SOURCES, MANAGE_INVENTORY_FOLDS, 'manage_inventory');
