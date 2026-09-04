/**
 * Timeline track records: add_track, add_section, remove_track,
 * list_tracks, list_track_types.
 *
 * Grounded in sequence-track-actions.ts and native
 * SequenceHandlersTrackCreation/TrackDiscovery/TrackRemoval/Sections.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';
const NR = 'Distinct Sequencer track operation with unique track type and lifecycle.';

export const TIMELINE_TRACKS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.add_track', action: 'add_track', family: F, domain: D,
    summary: 'Add a track of a specific type to a Level Sequence or binding.',
    whenToUse: ['A new track (transform, float, event, etc.) must be added.'],
    whenNotToUse: ['The track type is not supported by the sequence.'],
    inputProps: { action: P.action, path: P.path, trackType: P.trackType, trackName: P.trackName, actorName: P.actorName },
    required: ['action', 'path', 'trackType'],
    outputProps: { trackType: P.trackType, trackName: P.trackName, trackId: { type: 'string', description: 'Object name of the created track (addressable by later track actions).' }, trackClass: { type: 'string', description: 'UMovieSceneTrack subclass that was created.' }, trackPath: { type: 'string', description: 'Full object path of the created track.' }, actorName: P.actorName, bindingGuid: { type: 'string', description: 'Sequencer binding GUID the track was added to (bound tracks only).' } },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_track', path: '/Game/Cinematics/SEQ_Master', trackType: 'transform', actorName: 'Cube' },
    exampleOutput: { success: true, message: 'Track added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.add_section', action: 'add_section', family: F, domain: D,
    summary: 'Add a section to an existing track in a Level Sequence.',
    whenToUse: ['A section must be added to animate a sub-range of a track.'],
    whenNotToUse: ['The track does not exist.'],
    inputProps: { action: P.action, path: P.path, trackName: P.trackName, start: P.start, end: P.end },
    required: ['action', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_section', path: '/Game/Cinematics/SEQ_Master', trackName: 'Transform', start: 0, end: 60 },
    exampleOutput: { success: true, message: 'Section added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.remove_track', action: 'remove_track', family: F, domain: D,
    summary: 'Remove a track by name from a Level Sequence.',
    whenToUse: ['A track must be permanently removed from the sequence.'],
    whenNotToUse: ['The track should be muted instead.'],
    inputProps: { action: P.action, path: P.path, trackName: P.trackName },
    required: ['action', 'path', 'trackName'],
    effect: 'destructive', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'remove_track', path: '/Game/Cinematics/SEQ_Master', trackName: 'Fade' },
    exampleOutput: { success: true, message: 'Track removed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.list_tracks', action: 'list_tracks', family: F, domain: D,
    summary: 'List all tracks in a Level Sequence.',
    whenToUse: ['The tracks in a sequence must be enumerated.'],
    whenNotToUse: ['A specific track name is already known.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    // Native HandleListTracks (SequenceHandlersTrackDiscovery.cpp:38-131) emits
    // tracks as OBJECTS: master rows carry trackName/trackType/displayName/
    // isMasterTrack(=true)/sectionCount/isCameraCut (:72-82); binding rows add
    // bindingName/bindingGuid and isMasterTrack=false (:99-112). Also emits
    // trackCount and sequencePath at top level. Declared exactly — omitting
    // isCameraCut made every real response fail its own output schema.
    outputProps: {
      tracks: { type: 'array', items: { type: 'object', description: 'Track info.', additionalProperties: false, properties: {
        trackName: { type: 'string', description: 'Track name.' },
        trackType: { type: 'string', description: 'MovieScene track class name.' },
        displayName: { type: 'string', description: 'Track display name.' },
        isMasterTrack: { type: 'boolean', description: 'Whether the track belongs to the master (unbound) row.' },
        isCameraCut: { type: 'boolean', description: 'Whether the track is a camera-cut track.' },
        bindingName: { type: 'string', description: 'Bound actor name (binding tracks only).' },
        bindingGuid: { type: 'string', description: 'Bound object guid string (binding tracks only).' },
        sectionCount: { type: 'integer', description: 'Number of sections on the track.' },
      }, required: ['trackName', 'trackType', 'displayName', 'isMasterTrack', 'sectionCount'] }, description: 'Sequence tracks.' },
      trackCount: { type: 'integer', description: 'Total number of tracks reported.' },
      sequencePath: { type: 'string', description: 'Resolved sequence asset path.' },
    },
    outputRequired: ['tracks'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'list_tracks', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, tracks: [{ trackName: 'CameraCut', trackType: 'MovieSceneCameraCutTrack', displayName: 'CameraCut', isMasterTrack: true, sectionCount: 1 }], trackCount: 1 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.list_track_types', action: 'list_track_types', family: F, domain: D,
    summary: 'List all available MovieScene track types registered in the engine.',
    whenToUse: ['Available track types must be discovered before adding a track.'],
    whenNotToUse: ['The track type is already known.'],
    inputProps: { action: P.action },
    required: ['action'],
    outputProps: { types: { type: 'array', items: { type: 'string', description: 'Track type name.' }, description: 'Available track types.' } },
    outputRequired: ['types'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'list_track_types' },
    exampleOutput: { success: true, types: ['transform', 'float', 'event', 'camera_cut'] },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
