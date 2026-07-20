import type { CapabilityRecordSource } from '../../../index.js';
import { CHARACTER_1 } from './character-1.data.js';
import { CHARACTER_2 } from './character-2.data.js';

export const MANAGE_CHARACTER_SOURCES: readonly CapabilityRecordSource[] = [
  ...CHARACTER_1,
  ...CHARACTER_2,
];
