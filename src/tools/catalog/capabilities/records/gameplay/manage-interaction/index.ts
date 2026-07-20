import type { CapabilityRecordSource } from '../../../index.js';
import { INTERACTION_1 } from './interaction-1.data.js';
import { INTERACTION_2 } from './interaction-2.data.js';

export const MANAGE_INTERACTION_SOURCES: readonly CapabilityRecordSource[] = [
  ...INTERACTION_1,
  ...INTERACTION_2,
];
