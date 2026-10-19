// Fold specs for manage_sequence. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_SEQUENCE_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create', selector: 'sequenceOp',
    summary: 'Create a level sequence (sequenceOp create, the default): name in the folder path (default /Game), or path naming the asset itself; it is saved (saved), and a level sequence already there is kept and answered with alreadyExists, while another asset there is ASSET_ALREADY_EXISTS. duplicate copies the sequence at path into the folder destinationPath as newName and saves the copy; rename gives the sequence at path the name newName in its folder, references following it, and leaves it unsaved as asset.rename does. Only /Game paths are written (SEQUENCE_PATH_NOT_WRITABLE); a source that is not a level sequence is INVALID_SEQUENCE, and a taken destination ASSET_ALREADY_EXISTS.',
    topics: ['level sequence', 'create sequence', 'create cinematic', 'duplicate sequence', 'rename sequence'],
    members: { create: 'create', duplicate: 'duplicate', rename: 'rename' },
  },
  {
    primary: 'play', selector: 'control',
    summary: 'Play, pause or stop a level sequence in the editor; pause with startTime moves the playhead to that time and holds the frame there, so the level shows that moment.',
    topics: ['sequencer playhead', 'scrub sequence', 'set sequence time', 'go to frame', 'jump to time in sequence', 'hold a frame'],
    members: { play: 'play', pause: 'pause', stop: 'stop' },
  },
  {
    primary: 'edit_sequence_bindings', selector: 'edit',
    summary: 'Edit sequence bindings: add one or more actors, a camera, or a spawnable from a class; remove actors.',
    topics: ['sequence binding', 'add actor to sequence', 'spawnable', 'sequence camera', 'spawn actor in sequence'],
    members: { add_actor: 'add_actor', add_actors: 'add_actors', add_camera: 'add_camera', add_spawnable: 'add_spawnable_from_class', remove_actors: 'remove_actors' },
  },
  {
    primary: 'edit_sequence_tracks', selector: 'edit',
    summary: 'Edit sequence tracks: add a track, section or keyframe; move, trim or ease a section, or change the play rate of an animation clip; lock, mute or solo a track.',
    topics: ['sequence track', 'add track', 'add section', 'keyframe', 'mute track', 'solo track', 'lock track', 'keyframe actor location', 'crossfade clips', 'ease section', 'trim section', 'clip play rate'],
    members: { add_track: 'add_track', add_section: 'add_section', add_keyframe: 'add_keyframe', set_locked: 'set_track_locked', set_muted: 'set_track_muted', set_solo: 'set_track_solo', set_section: 'set_section' },
  },
  {
    primary: 'set_properties', selector: 'sequenceProperty',
    summary: 'Set sequence properties: playback range and frame rate, display rate, tick resolution, playback speed, view range, work range.',
    topics: ['frame rate', 'playback range', 'display rate', 'tick resolution', 'playback speed', 'work range', 'view range', 'cutscene length', 'level sequence length'],
    members: { properties: 'set_properties', display_rate: 'set_display_rate', tick_resolution: 'set_tick_resolution', playback_speed: 'set_playback_speed', view_range: 'set_view_range', work_range: 'set_work_range' },
  },
  {
    primary: 'delete', topics: ['delete level sequence', 'delete cutscene'], selector: 'deleteScope',
    summary: 'Delete a level sequence (deleteScope sequence, the default: refused with SEQUENCE_IN_USE, naming referencedBy, while a level or asset still uses it; its editors close and the undo history may be cleared), remove one track by name (track, one undo step), or remove keys (keyframe: from the tracks trackName or bindingId picks, on frame or frames, else every key on them; one undo step; replies removedKeys).',
    members: { sequence: 'delete', track: 'remove_track', keyframe: 'remove_keyframe' },
  },
  {
    primary: 'get_properties', selector: 'info',
    summary: 'Read a sequence: properties, bindings, tracks, the keys on a track, the available track types, list sequences, or open one in Sequencer.',
    topics: ['sequence properties', 'sequence bindings', 'list tracks', 'track types', 'list sequences', 'open sequencer'],
    members: { properties: 'get_properties', bindings: 'get_bindings', tracks: 'list_tracks', keys: 'list_track_keys', track_types: 'list_track_types', list: 'list', open: 'open' },
  },
  {
    primary: 'add_cinematic_track', selector: 'trackKind',
    summary: 'Add a cinematic track: camera cut, camera shake, transform, property, skeletal animation, material parameter, particle, event, fade, level visibility, shot, or subsequence.',
    topics: ['camera cut track', 'camera shake', 'transform track', 'property track', 'animation track', 'fade track', 'shot track', 'subsequence', 'fade to black', 'add skeletal animation to sequence'],
    members: {
      ...byTarget('add_', ['add_camera_cut_track', 'add_camera_shake_track', 'add_transform_track', 'add_property_track', 'add_skeletal_animation_track',
        'add_material_parameter_track', 'add_particle_track', 'add_event_track', 'add_fade_track', 'add_level_visibility_track', 'add_shot_track'], '_track'),
      subsequence: 'add_subsequence',
    },
  },
  {
    primary: 'configure_cinematic', selector: 'setting',
    summary: 'Configure cinematic cameras and shots: camera settings, crane or rail rigs, shot settings.',
    topics: ['cine camera settings', 'camera rig crane', 'camera rig rail', 'shot settings', 'focal length', 'aperture', 'set camera focal length'],
    members: { camera: 'configure_camera_settings', rig_crane: 'configure_camera_rig_crane', rig_rail: 'configure_camera_rig_rail', shot: 'configure_shot_settings' },
  },
  {
    primary: 'create_cinematic_asset', selector: 'kind',
    summary: 'Create a cinematic piece, chosen by kind. master_sequence makes a level sequence (name in path, default /Game/Cinematics, or sequencePath) at frameRate (default 24 fps) playing from startFrame (default 0) for durationFrames (default 240) or to endFrame, and saves it; it starts with no tracks, so add shots with add_shot_track and sub-sequences with add_subsequence. A level sequence already there is kept (alreadyExists); another asset there is ASSET_TYPE_MISMATCH. cine_camera_actor places a Cine Camera Actor named actorName (or label) at location and rotation with the lens, filmback and focus settings given (appliedProperties) and, with path or sequencePath, binds it to that sequence (bindingGuid); during Play it goes into the running game.',
    topics: ['cine camera actor', 'master sequence'],
    members: { cine_camera_actor: 'create_cine_camera_actor', master_sequence: 'create_master_sequence' },
  },
  {
    primary: 'create_render_job', selector: 'control',
    summary: 'Movie Render Queue: create a render job, queue it, start rendering, cancel a running render, or remove jobs from the queue.',
    topics: ['movie render queue', 'render job', 'queue render', 'start render', 'mrq', 'render video', 'remove render job', 'clear render queue', 'cancel render', 'stop render'],
    members: { create: 'create_render_job', queue: 'queue_render', start: 'start_render', cancel: 'cancel_render', remove: 'remove_render_job' },
  },
  {
    primary: 'configure_render_job', selector: 'setting',
    summary: 'Configure a Movie Render Queue job: output settings, anti-aliasing, render passes, burn-ins, console variables, and the camera setting (shutter timing, overscan).',
    topics: ['render output settings', 'anti aliasing', 'render pass', 'burn in', 'console variables', 'movie output resolution', 'shutter timing', 'motion blur at camera cuts', 'render overscan'],
    members: { output: 'configure_output_settings', anti_aliasing: 'configure_anti_aliasing', add_render_pass: 'add_render_pass', burn_ins: 'configure_burn_ins', console_variables: 'configure_console_variables', camera: 'configure_render_camera' },
  },
  {
    primary: 'create_media_asset', selector: 'kind',
    summary: 'Create a media asset: media source, player, playlist, texture, or sound component.',
    topics: ['media source', 'media player', 'media playlist', 'media texture', 'media sound'],
    members: byTarget('create_media_', ['create_media_source', 'create_media_player', 'create_media_playlist', 'create_media_texture', 'create_media_sound_component']),
  },
  {
    primary: 'play_media', topics: ['play video file'], selector: 'control',
    summary: 'Play a media player asset (control play, the default): it opens mediaSourcePath, a playlist (playlistPath, playlistIndex) or filePath first when given, closing what it was playing, else plays what it has open, and the reply waits up to 10 s for it to open and start (openStatus, openWaitMs, isPlaying, currentUrl), failing with MEDIA_OPEN_FAILED, MEDIA_OPEN_TIMEOUT or MEDIA_PLAY_TIMEOUT. Or pause it (pause) or seek it (seek). The picture shows only where its media texture is used (a material on a mesh, or a widget), and its sound needs a media sound component.',
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
    summary: 'Set how this editor session records and plays demo replays, chosen by setting. demo stores the default replayName and friendlyName, extra additionalOptions, maxRecordTimeSeconds (up to 86400), checkpointSaveMaxMSPerFrame (up to 1000 ms; also written to the engine config and the demo driver defaults), playbackSpeed, prioritizeActors and loadDefaultMapOnStop; a recording or playback running now takes them at once (appliedToRuntime), and the reply lists everything stored. killcam_duration sets how many seconds start_killcam replays (up to 600). All but the checkpoint budget last until the editor closes.',
    members: { demo: 'configure_demo_settings', killcam_duration: 'configure_killcam_duration' },
  },
  {
    primary: 'configure_take_recorder', selector: 'setting',
    summary: 'Take Recorder: configure sources and recorded tracks, open the panel, start or stop recording.',
    topics: ['take recorder', 'take sources', 'recorded tracks', 'start take recording', 'record actor animation'],
    members: { sources: 'configure_take_sources', recorded_tracks: 'configure_recorded_tracks', panel: 'create_take_recorder_panel', start_recording: 'start_recording', stop_recording: 'stop_recording' },
  },
];
