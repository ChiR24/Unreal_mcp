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
    description: 'Screenshot source: editor_viewport (default, the level viewport), game_viewport (the running game frame WITHOUT any UMG widgets) or full_editor_window (the editor window as displayed: the only mode that shows game UI and asset editors).'
  },
  // An asset editor (Widget Blueprint designer, material graph) is its own
  // window, so full_editor_window on the main frame alone could never show it.
  window: {
    type: 'string',
    description: 'With mode full_editor_window, which window to capture: a list index ("2") or a case-insensitive substring of its title ("WBP_HubUI"). Omit for the main editor frame, which is then always what is captured: a minimized main frame is restored without taking focus, and when that is not possible the call fails with EDITOR_WINDOW_MINIMIZED instead of capturing another window. Every response lists the open windows under windows[], so read that to pick one.'
  },
  // Camera and capture in one call: set_camera followed by a screenshot could return a frame drawn before the move.
  location: { ...P.location, description: 'editor_viewport: put the level viewport camera here first, in the same call ({x, y, z}); it stays there. Ignored while Play In Editor runs (the game camera is captured).' },
  rotation: { ...P.rotation, description: 'editor_viewport: turn the level viewport camera to this first ({pitch, yaw, roll}), in the same call.' },
  returnBase64: P.returnBase64,
  keepFile: {
    type: 'boolean',
    description: 'false: hand the image back without leaving a file in Saved/Screenshots (refused with returnBase64 false, which would leave no output at all). Default true. Files already written are listed by system_control list_output_files and removed by delete_output_file.'
  },
  includeMetadata: P.includeMetadata,
  metadata: P.metadata,
};

const SCREENSHOT_OUTPUT = {
  imageBase64: { type: 'string', description: 'Base64-encoded PNG image data.' },
  mimeType: { type: 'string', description: 'Image MIME type.' },
  width: { type: 'number', description: 'Width in pixels of the PNG actually returned.' },
  height: { type: 'number', description: 'Height in pixels of the PNG actually returned.' },
  viewportWidth: { type: 'number', description: 'Source viewport width in pixels. Present only when resolution forced a downscale, so width/height differ from the viewport.' },
  viewportHeight: { type: 'number', description: 'Source viewport height in pixels. Present only when resolution forced a downscale.' },
  sizeBytes: { type: 'integer', description: 'Image size in bytes.' },
  screenshotPath: { type: 'string', description: 'Saved screenshot file path.' },
  mode: { type: 'string', description: 'Screenshot source that was captured.' },
  window: { type: 'string', description: 'Title of the editor window that was actually captured.' },
  mainWindow: { type: 'boolean', description: 'full_editor_window: true when the captured window is the main editor frame.' },
  windows: {
    type: 'array',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    description: 'Every visible editor window: index, title, x, y, width, height, isActive, isModal. Pass an index or a title substring back as the window parameter to capture a different one; x/y are screen coordinates for simulate_input.'
  },
  windowCount: { type: 'number', description: 'Number of visible editor windows.' },
  cameraLocation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'editor_viewport: where the camera was for this picture.' },
  cameraRotation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'editor_viewport: how the camera was turned for this picture.' },
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
