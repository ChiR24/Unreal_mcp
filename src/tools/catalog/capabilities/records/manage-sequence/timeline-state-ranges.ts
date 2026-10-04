/**
 * Timeline track-state and range records: set_track_muted, set_track_solo,
 * set_track_locked, set_section, set_display_rate, set_tick_resolution, set_work_range,
 * set_view_range.
 *
 * Grounded in sequence-track-actions.ts (track state, ranges) and
 * sequence-playback-actions.ts (frame rate). Native bodies in
 * SequenceHandlersTrackState (with remove_track), SequenceHandlersRanges, SequenceHandlersFrameRate.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';

export const TIMELINE_STATE_RANGE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.set_track_muted', action: 'set_track_muted', family: F, domain: D,
    summary: 'Mute or unmute a track in a Level Sequence.',
    whenToUse: ['A track must be temporarily silenced.'],
    whenNotToUse: ['The track should be permanently removed.'],
    inputProps: { path: P.path, trackName: P.trackName, muted: { type: 'boolean', description: 'Whether to mute.' } },
    required: ['path', 'trackName'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_track_muted', path: '/Game/Cinematics/SEQ_Master', trackName: 'Audio', muted: true },
  }),
  buildRecord({
    id: 'sequence.set_track_solo', action: 'set_track_solo', family: F, domain: D,
    summary: 'Solo or unsolo a track in a Level Sequence.',
    whenToUse: ['A single track must be isolated for preview.'],
    whenNotToUse: ['Multiple tracks need to be audible simultaneously.'],
    inputProps: { path: P.path, trackName: P.trackName, solo: { type: 'boolean', description: 'Whether to solo.' } },
    required: ['path', 'trackName'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_track_solo', path: '/Game/Cinematics/SEQ_Master', trackName: 'Audio', solo: true },
  }),
  buildRecord({
    id: 'sequence.set_track_locked', action: 'set_track_locked', family: F, domain: D,
    summary: 'Lock or unlock a track in a Level Sequence.',
    whenToUse: ['A track must be protected from accidental edits.'],
    whenNotToUse: ['The track needs to be edited.'],
    inputProps: { path: P.path, trackName: P.trackName, locked: { type: 'boolean', description: 'Whether to lock.' } },
    required: ['path', 'trackName'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_track_locked', path: '/Game/Cinematics/SEQ_Master', trackName: 'Transform', locked: true },
  }),
  buildRecord({
    id: 'sequence.set_section', action: 'set_section', family: F, domain: D,
    summary: 'Move, trim or ease a section already on a track: its start and end frames, and the frames it eases in and out over.',
    whenToUse: ['Two animation clips should crossfade: overlap their sections, ease the later one in and the earlier one out.', 'A section must start or end on another frame.'],
    whenNotToUse: ['The section does not exist yet (use add_section, or add_cinematic_track for a clip).'],
    inputProps: {
      path: P.path,
      trackName: { type: 'string', description: 'The track, by the trackName get_properties info=tracks or info=keys lists (MovieSceneSkeletalAnimationTrack_3).' },
      sectionIndex: { type: 'integer', minimum: 0, description: 'Which section of the track, in the order info=keys lists them (default 0).' },
      startFrame: { type: 'number', description: 'New start of the section in display-rate frames.' },
      endFrame: { type: 'number', description: 'New end of the section in display-rate frames, after its start.' },
      easeInFrames: { type: 'number', minimum: 0, description: 'Frames the section fades in over; 0 removes its ease in.' },
      easeOutFrames: { type: 'number', minimum: 0, description: 'Frames the section fades out over; 0 removes its ease out.' },
    },
    required: ['path', 'trackName'],
    outputProps: { trackName: { type: 'string', description: 'The track the section is on.' }, sectionIndex: { type: 'integer', description: 'The section that changed.' } },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_section', path: '/Game/Cinematics/SEQ_Master', trackName: 'MovieSceneSkeletalAnimationTrack_3', endFrame: 130, easeOutFrames: 10 },
  }),
  buildRecord({
    id: 'sequence.set_display_rate', action: 'set_display_rate', family: F, domain: D,
    summary: 'Set the display frame rate of a Level Sequence.',
    whenToUse: ['The sequence display rate must be changed.'],
    whenNotToUse: ['The tick resolution needs changing instead.'],
    inputProps: { path: P.path, frameRate: P.frameRate },
    required: ['path', 'frameRate'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_display_rate', path: '/Game/Cinematics/SEQ_Master', frameRate: 24 },
  }),
  buildRecord({
    id: 'sequence.set_tick_resolution', action: 'set_tick_resolution', family: F, domain: D,
    summary: 'Set the tick resolution of a Level Sequence.',
    whenToUse: ['The sequence tick resolution must be changed.'],
    whenNotToUse: ['The display rate needs changing instead.'],
    inputProps: { path: P.path, resolution: P.resolution },
    required: ['path', 'resolution'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_tick_resolution', path: '/Game/Cinematics/SEQ_Master', resolution: '24000/1001' },
  }),
  buildRecord({
    id: 'sequence.set_work_range', action: 'set_work_range', family: F, domain: D,
    summary: 'Set the work range (editable region) of a Level Sequence.',
    whenToUse: ['The editable region must be bounded.'],
    whenNotToUse: ['The view range needs changing instead.'],
    inputProps: { path: P.path, start: P.start, end: P.end },
    required: ['path', 'start', 'end'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_work_range', path: '/Game/Cinematics/SEQ_Master', start: 0, end: 120 },
  }),
  buildRecord({
    id: 'sequence.set_view_range', action: 'set_view_range', family: F, domain: D,
    summary: 'Set the view range (visible region) of a Level Sequence.',
    whenToUse: ['The visible region in the Sequencer must be bounded.'],
    whenNotToUse: ['The work range needs changing instead.'],
    inputProps: { path: P.path, start: P.start, end: P.end },
    required: ['path'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_view_range', path: '/Game/Cinematics/SEQ_Master', start: 0, end: 60 },
  }),
];
