import type { CapabilityRecordSource } from '../../../index.js';
import { INVENTORY_1 } from './inventory-1.data.js';
import { INVENTORY_2 } from './inventory-2.data.js';

export const MANAGE_INVENTORY_SOURCES: readonly CapabilityRecordSource[] = [
  ...INVENTORY_1,
  ...INVENTORY_2,
];
