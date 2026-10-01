// Fold specs for control_editor. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const CONTROL_EDITOR_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'play', selector: 'control',
    summary: 'Control Play In Editor with control: play (starts it), pause, resume, stop, eject (leave the pawn for a free camera) or possess (take it back).',
    topics: ['play in editor', 'press play', 'start the game', 'run the game', 'start pie', 'stop pie', 'pause pie', 'pause game', 'pause simulation', 'resume game', 'eject', 'possess', 'quit game'],
    members: { play: 'play', pause: 'pause', resume: 'resume', stop: 'stop', eject: 'eject', possess: 'possess' },
    aliasMembers: { stop: 'stop_pie' },
  },
  {
    primary: 'set_game_speed', selector: 'control',
    summary: 'Control the clock of the running game during Play In Editor: game speed (slow motion), a fixed delta time, or step frames.',
    topics: ['game speed', 'time dilation', 'fixed delta time', 'step frame', 'slow motion', 'game too fast'],
    members: { speed: 'set_game_speed', fixed_delta_time: 'set_fixed_delta_time', step_frame: 'step_frame' },
    aliasMembers: { step_frame: 'single_frame_step' },
  },
  {
    primary: 'start_recording', selector: 'control',
    summary: 'Start or stop an editor gameplay recording.',
    members: { start: 'start_recording', stop: 'stop_recording' },
  },
  {
    primary: 'set_camera', selector: 'cameraOp',
    summary: 'Set the viewport camera transform, its field of view, or the view target actor. While Play In Editor runs, the transform moves the free camera of an ejected player and is refused until the player is ejected.',
    topics: ['viewport camera', 'camera position', 'camera fov', 'view target', 'look at actor', 'move the camera', 'move camera', 'game camera', 'pie camera'],
    members: { transform: 'set_camera', fov: 'set_camera_fov', view_target: 'set_view_target' },
    aliasMembers: { transform: ['set_camera_position', 'set_viewport_camera'], view_target: 'set_game_view_target' },
  },
  {
    primary: 'configure_viewport', selector: 'setting',
    summary: 'Configure the editor viewport: view mode, editor mode, game view, immersive mode, realtime, show or hide stats.',
    topics: ['view mode', 'editor mode', 'game view', 'immersive mode', 'realtime viewport', 'viewport stats', 'show stats'],
    members: { view_mode: 'set_view_mode', editor_mode: 'set_editor_mode', game_view: 'set_game_view', immersive_mode: 'set_immersive_mode', realtime: 'set_viewport_realtime', show_stats: 'show_stats', hide_stats: 'hide_stats' },
  },
  { primary: 'console_command', summary: 'Run a validated console command in the editor.', members: ['execute_command'] },
  {
    primary: 'configure_editor', selector: 'setting',
    summary: 'Open an editor tab, set editor preferences, or restore or minimize the main editor window without focus (restoring unthrottles Play In Editor).',
    topics: ['editor tab', 'editor preferences', 'open tab', 'restore editor window', 'minimize editor window', 'editor minimized', 'pie slow 3 fps', 'throttle'],
    members: { open_tab: 'open_editor_tab', preferences: 'set_preferences', window: 'restore_editor_window' },
  },
  {
    primary: 'screenshot', summary: 'Capture a screenshot of the level viewport (optionally from a camera location and rotation given in the same call), of the running game view, or of the whole editor window with the game UI.', members: ['take_screenshot'],
    topics: ['capture viewport', 'screen capture', 'viewport image', 'snapshot', 'take picture', 'look from here', 'camera screenshot', 'game view screenshot', 'screenshot hud'],
  },
  {
    primary: 'undo', selector: 'history',
    summary: 'Undo or redo the last editor transaction; the reply names the transaction, or says there was none.',
    topics: ['undo', 'redo', 'transaction history'],
    members: { undo: 'undo', redo: 'redo' },
  },
];
