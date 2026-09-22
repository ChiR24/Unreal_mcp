/**
 * Barrel for the global/runtime inspection records. Re-exports the three
 * concept modules in their original concatenation order so
 * GLOBAL_RUNTIME_RECORDS is unchanged.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { EDITOR_SETTINGS_RECORD, GLOBAL_RECORDS } from './global.data.js';
import { RUNTIME_RECORDS } from './runtime.data.js';
import { STATS_RECORDS } from './stats.data.js';

export const GLOBAL_RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  ...RUNTIME_RECORDS,
  ...GLOBAL_RECORDS,
  ...STATS_RECORDS,
  EDITOR_SETTINGS_RECORD,
];
