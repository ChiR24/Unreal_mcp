/**
 * Timeline track records: add_track, add_section, remove_track,
 * list_tracks, list_track_types.
 *
 * Grounded in sequence-track-actions.ts and native
 * SequenceHandlersTrackCreation/TrackDiscovery/TrackRemoval/Sections.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';

export const TIMELINE_TRACKS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.add_track', action: 'add_track', family: F, domain: D,
    summary: 'Add a track of a specific type to a Level Sequence or binding.',
    whenToUse: ['A new track (transform, float, event, etc.) must be added.'],
    whenNotToUse: ['The track type is not supported by the sequence.'],
    inputProps: { path: P.path, trackType: P.trackType, trackName: P.trackName, actorName: P.actorName },
    required: ['path', 'trackType'],
    outputProps: { trackType: P.trackType, trackName: P.trackName, trackId: { type: 'string', description: 'Object name of the created track (addressable by later track actions).' }, trackClass: { type: 'string', description: 'UMovieSceneTrack subclass that was created.' }, trackPath: { type: 'string', description: 'Full object path of the created track.' }, actorName: P.actorName, bindingGuid: { type: 'string', description: 'Sequencer binding GUID the track was added to (bound tracks only).' } },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_track', path: '/Game/Cinematics/SEQ_Master', trackType: 'transform', actorName: 'Cube' },
  }),
  buildRecord({
    id: 'sequence.add_section', action: 'add_section', family: F, domain: D,
    summary: 'Add a section to an existing track in a Level Sequence; on an Audio track, soundPath puts music or a sound in it.',
    topics: ['add music to sequence', 'add sound to cutscene', 'sequence audio'],
    whenToUse: ['A section must be added to animate a sub-range of a track.',
      'Music or a sound must play in a cutscene: add an Audio track, then a section with soundPath.'],
    whenNotToUse: ['The track does not exist.', 'A sound must play in the running game, not inside a sequence.'],
    // Native HandleSequenceAddSection (SequenceHandlersSections.cpp) filters the
    // track by actorName's binding and needs bindingId for a camera cut section.
    inputProps: { path: P.path, trackName: P.trackName, actorName: { type: 'string', description: 'Only search the tracks of this bound actor.' }, bindingId: { type: 'string', description: 'Camera binding GUID for a camera cut section (required on a camera cut track).' }, soundPath: { type: 'string', description: 'Audio track only: /Game path of the SoundWave, SoundCue or MetaSound the section plays. Without end the section is as long as the sound (one second for a looping one).' }, start: { type: 'number', description: 'Section start in display-rate frames (default 0).' }, end: { type: 'number', description: 'Section end in display-rate frames, after start (default 100; with soundPath, the length of the sound).' } },
    required: ['path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_section', path: '/Game/Cinematics/SEQ_Master', trackName: 'Transform', start: 0, end: 60 },
  }),
  buildRecord({
    id: 'sequence.remove_track', action: 'remove_track', family: F, domain: D,
    summary: 'Remove a track by name from a Level Sequence.',
    whenToUse: ['A track must be permanently removed from the sequence.'],
    whenNotToUse: ['The track should be muted instead.'],
    inputProps: { path: P.path, trackName: P.trackName },
    required: ['path', 'trackName'],
    effect: 'destructive', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'remove_track', path: '/Game/Cinematics/SEQ_Master', trackName: 'Fade' },
  }),
  buildRecord({
    id: 'sequence.list_tracks', action: 'list_tracks', family: F, domain: D,
    summary: 'List all tracks in a Level Sequence.',
    whenToUse: ['The tracks in a sequence must be enumerated.'],
    whenNotToUse: ['A specific track name is already known.'],
    inputProps: { path: P.path },
    required: ['path'],
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
  }),
  buildRecord({
    id: 'sequence.list_track_types', action: 'list_track_types', family: F, domain: D,
    summary: 'List all available MovieScene track types registered in the engine.',
    whenToUse: ['Available track types must be discovered before adding a track.'],
    whenNotToUse: ['The track type is already known.'],
    inputProps: {},
    outputProps: { types: { type: 'array', items: { type: 'string', description: 'Track type name.' }, description: 'Available track types.' } },
    outputRequired: ['types'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'list_track_types' },
    exampleOutput: { success: true, types: ['transform', 'float', 'event', 'camera_cut'] },
  }),
  buildRecord({
    id: 'sequence.list_track_keys', action: 'list_track_keys', family: F, domain: D,
    summary: 'Read the sections and keyframes already on a sequence track.',
    whenToUse: ['Existing keys must be inspected before re-authoring a track.',
      'A keyframe appeared to do nothing and the section range needs checking.'],
    whenNotToUse: ['Only track names are needed; list_tracks is cheaper.'],
    inputProps: { path: P.path, trackName: P.trackName },
    required: ['path'],
    // Frames come back in DISPLAY units because every other keyframe
    // capability speaks display frames; a readback in ticks would be its own
    // trap. rangeIsEmpty is the load-bearing field: keys inside an
    // empty-range section cover no time and never evaluate, which is exactly
    // how a keyframe can look applied and change nothing.
    outputProps: {
      trackKeys: { type: 'array', description: 'Tracks with their sections and keys.', items: {
        type: 'object', additionalProperties: false, description: 'Track keys.', properties: {
          trackName: { type: 'string', description: 'Track name.' },
          trackType: { type: 'string', description: 'MovieScene track class name.' },
          bindingName: { type: 'string', description: 'The bound actor or component the track animates; absent on a master track.' },
          sections: { type: 'array', description: 'Sections on the track.', items: {
            type: 'object', additionalProperties: false, description: 'Section keys.', properties: {
              sectionName: { type: 'string', description: 'Section object name.' },
              rangeIsEmpty: { type: 'boolean', description: 'True when the section covers no time, so its keys never evaluate.' },
              startFrame: { type: 'number', description: 'Section start in display frames.' },
              endFrame: { type: 'number', description: 'Section end in display frames.' },
              rowIndex: { type: 'integer', description: 'Row of the section within its track.' },
              easeInFrames: { type: 'number', description: 'Frames the section fades in over; absent when it does not ease in.' },
              easeOutFrames: { type: 'number', description: 'Frames the section fades out over; absent when it does not ease out.' },
              animation: { type: 'string', description: 'The clip an animation section plays.' },
              channels: { type: 'array', description: 'Channels and their keys.', items: {
                type: 'object', additionalProperties: false, description: 'Channel keys.', properties: {
                  channelIndex: { type: 'integer', description: 'Index within the channel family.' },
                  channelType: { type: 'string', description: 'double, float or bool.' },
                  channelName: { type: 'string', description: 'Channel name, when the section publishes metadata.' },
                  keyCount: { type: 'integer', description: 'Number of keys on the channel.' },
                  keys: { type: 'array', description: 'Keys on the channel.', items: {
                    type: 'object', additionalProperties: false, description: 'One key.', properties: {
                      frame: { type: 'number', description: 'Key time in display frames.' },
                      value: { type: 'number', description: 'Key value; a bool key (a Visibility track) reads 1 for true and 0 for false.' },
                    } } },
                } } },
            } } },
        } } },
      trackCount: { type: 'integer', description: 'Tracks reported.' },
      keyCount: { type: 'integer', description: 'Total keys across every reported channel.' },
      sequencePath: { type: 'string', description: 'Resolved sequence asset path.' },
    },
    outputRequired: ['trackKeys'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'list_track_keys', path: '/Game/Cinematics/SEQ_Master', trackName: 'MovieScene3DTransformTrack_0' },
    exampleOutput: { success: true, trackKeys: [{ trackName: 'MovieScene3DTransformTrack_0', trackType: 'MovieScene3DTransformTrack', sections: [{ sectionName: 'MovieScene3DTransformSection_0', rangeIsEmpty: false, startFrame: 0, endFrame: 120, channels: [] }] }], trackCount: 1, keyCount: 15 },
  }),
  buildRecord({
    id: 'sequence.remove_keyframe', action: 'remove_keyframe', family: F, domain: D,
    summary: 'Remove keyframes from a sequence track: one frame, or every key on the track.',
    whenToUse: ['A track must be cleanly re-authored rather than added to.',
      'A single bad key must be deleted.'],
    whenNotToUse: ['The whole track should go; use delete with deleteScope track.'],
    inputProps: { path: P.path, trackName: P.trackName, bindingId: P.bindingId, frame: P.frame },
    required: ['path'],
    // Omitting `frame` clears every key on the matching track, which is the
    // operation wanted before re-authoring. removedKeys is reported because
    // "removed 0" and "removed 12" must not read the same.
    outputProps: {
      matchedTracks: { type: 'integer', description: 'Tracks the filter matched.' },
      removedKeys: { type: 'integer', description: 'Keys actually removed.' },
      clearedAllFrames: { type: 'boolean', description: 'True when no frame was supplied and the whole track was cleared.' },
      sequencePath: { type: 'string', description: 'Resolved sequence asset path.' },
    },
    outputRequired: ['removedKeys'],
    effect: 'destructive', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'remove_keyframe', path: '/Game/Cinematics/SEQ_Master', trackName: 'MovieScene3DTransformTrack_0' },
    exampleOutput: { success: true, message: 'Removed 15 key(s) from 1 track(s)', matchedTracks: 1, removedKeys: 15, clearedAllFrames: true },
  }),
];
