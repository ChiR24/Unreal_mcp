/**
 * manage_sequence capability record catalog: timeline, metadata, cinematic,
 * MRQ, media, take recorder and replay families, folded by MANAGE_SEQUENCE_FOLDS.
 */
import { type CapabilityRecordSource } from '../../model.js';

import { CINEMATIC_RECORDS } from './cinematic.js';
import { MEDIA_RECORDS } from './media.js';
import { METADATA_RECORDS } from './metadata.js';
import { MRQ_RECORDS } from './mrq.js';
import { REPLAY_RECORDS } from './replay.js';
import { TAKE_RECORDS } from './take.js';
import { TIMELINE_BINDINGS_RECORDS } from './timeline-bindings.js';
import { TIMELINE_LIFECYCLE_RECORDS } from './timeline-lifecycle.js';
import { TIMELINE_PLAYBACK_RECORDS } from './timeline-playback.js';
import { TIMELINE_STATE_RANGE_RECORDS } from './timeline-state-ranges.js';
import { TIMELINE_TRACKS_RECORDS } from './timeline-tracks.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_SEQUENCE_FOLDS } from '../folds/manage-sequence.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_SEQUENCE_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...TIMELINE_LIFECYCLE_RECORDS,
  ...TIMELINE_PLAYBACK_RECORDS,
  ...TIMELINE_BINDINGS_RECORDS,
  ...TIMELINE_TRACKS_RECORDS,
  ...TIMELINE_STATE_RANGE_RECORDS,
  ...METADATA_RECORDS,
  ...CINEMATIC_RECORDS,
  ...MRQ_RECORDS,
  ...MEDIA_RECORDS,
  ...TAKE_RECORDS,
  ...REPLAY_RECORDS,
];

export const MANAGE_SEQUENCE_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_SEQUENCE_UNFOLDED_SOURCES, MANAGE_SEQUENCE_FOLDS, 'manage_sequence');

