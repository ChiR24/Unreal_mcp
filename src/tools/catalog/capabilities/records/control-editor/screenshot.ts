/**
 * Screenshot records: screenshot, take_screenshot.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'screenshot';
const D = 'editor';

const SCREENSHOT_PROPS = {
  filename: P.filename,
  path: { type: 'string', description: 'Directory to save the PNG in, inside the project (relative to it, or absolute under it). Default Saved/Screenshots (game_viewport: Saved/Screenshots/WindowsEditor). A path outside the project is refused.' },
  // Not P.resolution: this is a resample of one already-rendered frame, not a
  // re-render, so WxH is a bounding box rather than an exact output size --
  // aspect ratio is preserved and a box larger than the frame changes nothing.
  // It is also the documented way out of IMAGE_TOO_LARGE.
  resolution: {
    type: 'string',
    description: 'Maximum WxH for the returned PNG (e.g. "1280x720"). The capture is downscaled to fit inside this box with its aspect ratio preserved; a box at least as large as the viewport leaves the image untouched. Without it an image returned inline fits 1600x900, and a file-only capture (returnBase64 false) keeps the viewport size.'
  },
  // P.mode is shared with set_editor_mode and named no screenshot source, so
  // the one choice that decides whether UMG shows up was undiscoverable.
  mode: {
    type: 'string',
    enum: ['editor_viewport', 'game_viewport', 'full_editor_window'],
    description: 'Screenshot source: editor_viewport (default, the level viewport), game_viewport (the running game frame WITHOUT any UMG widgets) or full_editor_window (the editor window as displayed: the only mode that shows game UI and asset editors). While Play In Editor runs with the player ejected (control_editor play control=eject), the game is drawn by the editor viewport, so editor_viewport and game_viewport are then the same picture: the ejected view.'
  },
  // An asset editor (Widget Blueprint designer, material graph) is its own
  // window, so full_editor_window on the main frame alone could never show it.
  window: {
    type: ['integer', 'string'],
    description: 'With mode full_editor_window, which window to capture: a list index (2, or "2") or a case-insensitive substring of its title ("WBP_HubUI"). Omit for the main editor frame, which is then always what is captured: a minimized main frame is restored without taking focus, and when that is not possible the call fails with EDITOR_WINDOW_MINIMIZED instead of capturing another window. A window restored for the capture is minimized again right after it. Every response lists the open windows under windows[], so read that to pick one.'
  },
  // Camera and capture in one call: set_camera followed by a screenshot could return a frame drawn before the move.
  location: { ...P.location, description: 'editor_viewport (and game_viewport while the player is ejected): put the camera here first, in the same call ({x, y, z}); it stays there. That is the level viewport camera, or, while Play In Editor runs with the player ejected, the ejected view. While Play In Editor runs and the player is not ejected there is no free camera to move and the call is refused with PIE_VIEW_NOT_EJECTED (eject first, or point the game camera with set_camera cameraOp=view_target), exactly as set_camera is.' },
  rotation: { ...P.rotation, description: 'editor_viewport (and game_viewport while the player is ejected): turn that camera to this first ({pitch, yaw, roll}), in the same call. Refused like location while Play In Editor runs and the player is not ejected.' },
  returnBase64: P.returnBase64,
  keepFile: {
    type: 'boolean',
    description: 'false: hand the image back without leaving a file in Saved/Screenshots (refused with returnBase64 false, which would leave no output at all). Default true. Files already written are listed by system_control list_output_files and removed by delete_output_file.'
  },
  // Shaders recompile after a scalability or material change; until they finish the picture shows the engine's default
  // material (black foliage, grey ground) and nothing in it says so.
  waitForShaders: {
    type: 'boolean',
    description: 'true: when shaders are still compiling (after a scalability or material change), wait for the queue to drain before capturing, polling without blocking the game thread, for at most 25 seconds; shaderWait in the reply says how long it waited and how many jobs were left. Default false: capture now and report shadersCompiling.'
  },
  includeMetadata: P.includeMetadata,
  metadata: P.metadata,
};

const SCREENSHOT_OUTPUT = {
  shadersCompiling: { type: 'number', description: 'Shader compile jobs still outstanding when the picture was taken. Above 0, the surfaces they cover are drawn with the engine default material (black foliage, grey ground) and a warning says so; pass waitForShaders to capture after they finish.' },
  shaderWait: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'With waitForShaders while shaders compiled: waitedSeconds, jobsLeft (still outstanding when the wait ended) and timedOut (true when jobs were left after the 25 second cap).' },
  imageBase64: { type: 'string', description: 'Base64-encoded PNG image data.' },
  mimeType: { type: 'string', description: 'Image MIME type.' },
  width: { type: 'number', description: 'Width in pixels of the PNG actually returned.' },
  height: { type: 'number', description: 'Height in pixels of the PNG actually returned.' },
  viewportWidth: { type: 'number', description: 'Width in pixels of the viewport (or, for full_editor_window, the window) the picture was taken at. Present only when the image was downscaled (a resolution, or the 1600x900 inline default), so width/height differ from it.' },
  viewportHeight: { type: 'number', description: 'Height in pixels of the viewport or window the picture was taken at. Present only when the image was downscaled.' },
  sizeBytes: { type: 'integer', description: 'Image size in bytes.' },
  screenshotPath: { type: 'string', description: 'Saved screenshot file path.' },
  mode: { type: 'string', description: 'Screenshot source that was captured.' },
  window: { type: 'string', description: 'Title of the editor window that was actually captured.' },
  mainWindow: { type: 'boolean', description: 'full_editor_window: true when the captured window is the main editor frame.' },
  windowRestored: { type: 'boolean', description: 'full_editor_window: true when the window was minimized and was put back on screen, without taking focus, for the capture. It is minimized again right after the capture, so windows[] shows it minimized.' },
  windows: {
    type: 'array',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    description: 'Every visible editor window: index, title, x, y, width, height, isActive, isMinimized, isModal. Pass an index or a title substring back as the window parameter to capture a different one; x/y are screen coordinates for simulate_input.'
  },
  windowCount: { type: 'number', description: 'Number of visible editor windows.' },
  cameraLocation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'editor_viewport and an ejected game view: where the camera was for this picture.' },
  cameraRotation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'editor_viewport and an ejected game view: how the camera was turned for this picture.' },
  view: { type: 'string', description: 'Which view the picture is of: editor_viewport (the level viewport), pie_game (the running game as its possessed pawn sees it) or pie_ejected (the free camera of an ejected player). Absent for full_editor_window and for the game_viewport capture of a possessed game.' },
};

export const SCREENSHOT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'screenshot', domain: D, family: F,
    summary: 'Capture a screenshot from the editor viewport, game viewport, or full editor window.',
    whenToUse: ['A visual snapshot of the editor or game viewport is needed.'],
    whenNotToUse: ['A real-time capture stream is needed.'],
    inputProps: SCREENSHOT_PROPS,
    required: [],
    outputProps: SCREENSHOT_OUTPUT,
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'screenshot', mode: 'editor_viewport', filename: 'viewport' },
    exampleOutput: { success: true, screenshotPath: '/Game/Screenshots/viewport.png', mode: 'editor_viewport' },
  }),
];
