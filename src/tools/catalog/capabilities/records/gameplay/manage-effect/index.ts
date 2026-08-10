import type { CapabilityRecordSource } from '../../../index.js';
import { EFFECT_1 } from './effect-1.data.js';
import { EFFECT_2 } from './effect-2.data.js';
import { EFFECT_3 } from './effect-3.data.js';

export const MANAGE_EFFECT_SOURCES: readonly CapabilityRecordSource[] = [
  ...EFFECT_1,
  ...EFFECT_2,
  ...EFFECT_3,
];
