// Fold specs for manage_sequence. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_SEQUENCE_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create', selector: 'sequenceOp',
    summary: 'Create a level sequence, or duplicate or rename one.',
    topics: ['level sequence', 'create sequence', 'create cinematic', 'duplicate sequence', 'rename sequence'],
    members: { create: 'create', duplicate: 'duplicate', rename: 'rename' },
  },
  {
    primary: 'play', selector: 'control',
    summary: 'Play, pause or stop a level sequence in the editor.',
    members: { play: 'play', pause: 'pause', stop: 'stop' },
  },
  {
    primary: 'edit_sequence_bindings', selector: 'edit',
    summary: 'Edit sequence bindings: add one or more actors, a camera, or a spawnable from a class; remove actors.',
    topics: ['sequence binding', 'add actor to sequence', 'spawnable', 'sequence camera'],
    members: { add_actor: 'add_actor', add_actors: 'add_actors', add_camera: 'add_camera', add_spawnable: 'add_spawnable_from_class', remove_actors: 'remove_actors' },
  },
  {
    primary: 'edit_sequence_tracks', selector: 'edit',
    summary: 'Edit sequence tracks: add a track, section or keyframe; lock, mute or solo a track.',
    topics: ['sequence track', 'add track', 'add section', 'keyframe', 'mute track', 'solo track', 'lock track'],
    members: { add_track: 'add_track', add_section: 'add_section', add_keyframe: 'add_keyframe', set_locked: 'set_track_locked', set_muted: 'set_track_muted', set_solo: 'set_track_solo' },
  },
  {
    primary: 'set_properties', selector: 'sequenceProperty',
    summary: 'Set sequence properties: playback range and frame rate, display rate, tick resolution, playback speed, view range, work range.',
    topics: ['frame rate', 'playback range', 'display rate', 'tick resolution', 'playback speed', 'work range', 'view range'],
    members: { properties: 'set_properties', display_rate: 'set_display_rate', tick_resolution: 'set_tick_resolution', playback_speed: 'set_playback_speed', view_range: 'set_view_range', work_range: 'set_work_range' },
  },
  {
    primary: 'delete', selector: 'deleteScope',
    summary: 'Delete a level sequence, or remove one of its tracks.',
    members: { sequence: 'delete', track: 'remove_track' },
  },
  {
    primary: 'get_properties', selector: 'info',
    summary: 'Read a sequence: properties, bindings, tracks, the available track types, list sequences, or open one in Sequencer.',
    topics: ['sequence properties', 'sequence bindings', 'list tracks', 'track types', 'list sequences', 'open sequencer'],
    members: { properties: 'get_properties', bindings: 'get_bindings', tracks: 'list_tracks', track_types: 'list_track_types', list: 'list', open: 'open' },
  },
  {
    primary: 'add_cinematic_track', selector: 'trackKind',
    summary: 'Add a cinematic track: camera cut, camera shake, transform, property, skeletal animation, material parameter, particle, event, fade, level visibility, shot, or subsequence.',
    topics: ['camera cut track', 'camera shake', 'transform track', 'property track', 'animation track', 'fade track', 'shot track', 'subsequence'],
    members: {
      ...byTarget('add_', ['add_camera_cut_track', 'add_camera_shake_track', 'add_transform_track', 'add_property_track', 'add_skeletal_animation_track',
        'add_material_parameter_track', 'add_particle_track', 'add_event_track', 'add_fade_track', 'add_level_visibility_track', 'add_shot_track'], '_track'),
      subsequence: 'add_subsequence',
    },
  },
  {
    primary: 'configure_cinematic', selector: 'setting',
    summary: 'Configure cinematic cameras and shots: camera settings, crane or rail rigs, shot settings.',
    topics: ['cine camera settings', 'camera rig crane', 'camera rig rail', 'shot settings', 'focal length', 'aperture'],
    members: { camera: 'configure_camera_settings', rig_crane: 'configure_camera_rig_crane', rig_rail: 'configure_camera_rig_rail', shot: 'configure_shot_settings' },
  },
  {
    primary: 'create_cinematic_asset', selector: 'kind',
    summary: 'Create a cine camera actor or a master sequence.',
    topics: ['cine camera actor', 'master sequence'],
    members: { cine_camera_actor: 'create_cine_camera_actor', master_sequence: 'create_master_sequence' },
  },
  {
    primary: 'create_render_job', selector: 'control',
    summary: 'Movie Render Queue: create a render job, queue it, or start rendering.',
    topics: ['movie render queue', 'render job', 'queue render', 'start render', 'mrq'],
    members: { create: 'create_render_job', queue: 'queue_render', start: 'start_render' },
  },
  {
    primary: 'configure_render_job', selector: 'setting',
    summary: 'Configure a Movie Render Queue job: output settings, anti-aliasing, render passes, burn-ins, console variables.',
    topics: ['render output settings', 'anti aliasing', 'render pass', 'burn in', 'console variables'],
    members: { output: 'configure_output_settings', anti_aliasing: 'configure_anti_aliasing', add_render_pass: 'add_render_pass', burn_ins: 'configure_burn_ins', console_variables: 'configure_console_variables' },
  },
  {
    primary: 'create_media_asset', selector: 'kind',
    summary: 'Create a media asset: media source, player, playlist, texture, or sound component.',
    topics: ['media source', 'media player', 'media playlist', 'media texture', 'media sound'],
    members: byTarget('create_media_', ['create_media_source', 'create_media_player', 'create_media_playlist', 'create_media_texture', 'create_media_sound_component']),
  },
  {
    primary: 'play_media', selector: 'control',
    summary: 'Play, pause or seek a media player.',
    members: { play: 'play_media', pause: 'pause_media', seek: 'seek_media' },
  },
  {
    primary: 'play_demo', selector: 'control',
    summary: 'Replay system: play, pause or seek a demo, set playback speed, start or stop recording, start a killcam.',
    topics: ['replay', 'demo recording', 'play demo', 'killcam', 'demo playback speed'],
    members: { play: 'play_demo', pause: 'pause_demo', seek: 'seek_demo', set_playback_speed: 'set_demo_playback_speed', start_recording: 'start_demo_recording', stop_recording: 'stop_demo_recording', start_killcam: 'start_killcam' },
  },
  {
    primary: 'configure_demo_settings', selector: 'setting',
    summary: 'Configure replay demo settings or the killcam duration.',
    members: { demo: 'configure_demo_settings', killcam_duration: 'configure_killcam_duration' },
  },
  {
    primary: 'configure_take_recorder', selector: 'setting',
    summary: 'Take Recorder: configure sources and recorded tracks, open the panel, start or stop recording.',
    topics: ['take recorder', 'take sources', 'recorded tracks', 'start take recording'],
    members: { sources: 'configure_take_sources', recorded_tracks: 'configure_recorded_tracks', panel: 'create_take_recorder_panel', start_recording: 'start_recording', stop_recording: 'stop_recording' },
  },
];
