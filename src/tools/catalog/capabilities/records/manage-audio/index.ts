import { compareById } from '../../../../../utils/serialization/ordering.js';
import { createCapabilityRecord, type CapabilityRecord, type CapabilityRecordSource } from '../../index.js';
import { AUDIO_AUTHORING_RECORDS } from './authoring.data.js';
import { AUDIO_RUNTIME_RECORDS } from './runtime.data.js';
import { MANAGE_AUDIO_FOLDS } from '../folds/manage-audio.folds.js';
import { applyFolds } from '../shared/fold.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_AUDIO_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...AUDIO_RUNTIME_RECORDS,
  ...AUDIO_AUTHORING_RECORDS,
];

export const MANAGE_AUDIO_SOURCES: readonly CapabilityRecordSource[] = Object.freeze(applyFolds(MANAGE_AUDIO_UNFOLDED_SOURCES, MANAGE_AUDIO_FOLDS, 'manage_audio'));

export const MANAGE_AUDIO_RECORDS: readonly CapabilityRecord[] = Object.freeze(
  [...MANAGE_AUDIO_SOURCES.map((source) => createCapabilityRecord(source))]
    .sort(compareById),
);

export const MANAGE_AUDIO_RECORD_COUNT = MANAGE_AUDIO_RECORDS.length;
