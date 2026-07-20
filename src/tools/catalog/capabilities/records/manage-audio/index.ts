import { createCapabilityRecord, type CapabilityRecord, type CapabilityRecordSource } from '../../index.js';
import { AUDIO_AUTHORING_RECORDS } from './authoring.data.js';
import { AUDIO_RUNTIME_RECORDS } from './runtime.data.js';

export const MANAGE_AUDIO_SOURCES: readonly CapabilityRecordSource[] = Object.freeze([
  ...AUDIO_RUNTIME_RECORDS,
  ...AUDIO_AUTHORING_RECORDS,
]);

export const MANAGE_AUDIO_RECORDS: readonly CapabilityRecord[] = Object.freeze(
  [...MANAGE_AUDIO_SOURCES.map((source) => createCapabilityRecord(source))]
    .sort((left, right) => left.id.localeCompare(right.id)),
);

export const MANAGE_AUDIO_RECORD_COUNT = MANAGE_AUDIO_RECORDS.length;
