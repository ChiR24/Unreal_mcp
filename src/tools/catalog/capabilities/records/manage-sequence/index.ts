/**
 * manage_sequence capability record catalog.
 *
 * Exactly 81 canonical CapabilityRecord entries mapped 1:1 to the 81
 * manage_sequence actions in manage-sequence-tool.ts. Each record is grounded
 * in the TypeScript handler bodies, native C++ Sequence domain dispatch,
 * and the audio/sequence/system closeout evidence.
 *
 * Families (6 + timeline base):
 * - timeline (31): lifecycle, playback, bindings, tracks, track-state, ranges
 * - metadata (2): get_metadata, set_metadata (cross-parent to Level domain)
 * - cinematic (18): CINEMATICS_ACTIONS
 * - mrq (8): MOVIE_RENDER_ACTIONS (async/cancellation/artifact contracts)
 * - media (8): MEDIA_ACTIONS (ElectraPlayer gate)
 * - take (5): Take Recorder (async, no cancel, artifact = LevelSequence)
 * - replay (9): Demo/killcam (async, no cancel, artifact = replay files)
 *
 * Total: 31 + 2 + 18 + 8 + 8 + 5 + 9 = 81
 */
import { type CapabilityRecord, type CapabilityRecordSource, createCapabilityRecord } from '../../index.js';

import { CINEMATIC_RECORDS_A } from './cinematic-a.js';
import { CINEMATIC_RECORDS_B } from './cinematic-b.js';
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

const SOURCES: readonly CapabilityRecordSource[] = [
  ...TIMELINE_LIFECYCLE_RECORDS,
  ...TIMELINE_PLAYBACK_RECORDS,
  ...TIMELINE_BINDINGS_RECORDS,
  ...TIMELINE_TRACKS_RECORDS,
  ...TIMELINE_STATE_RANGE_RECORDS,
  ...METADATA_RECORDS,
  ...CINEMATIC_RECORDS_A,
  ...CINEMATIC_RECORDS_B,
  ...MRQ_RECORDS,
  ...MEDIA_RECORDS,
  ...TAKE_RECORDS,
  ...REPLAY_RECORDS,
];

export const MANAGE_SEQUENCE_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const MANAGE_SEQUENCE_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const MANAGE_SEQUENCE_RECORD_COUNT = MANAGE_SEQUENCE_RECORDS.length;
