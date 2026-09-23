/**
 * Shared JSON-schema property fragments for control_editor capability records.
 *
 * Mirrors the canonical control_editor tool definition parameter shapes from
 * src/tools/definitions/core/control-editor-tool.ts and the allowed-params
 * contract in src/tools/handlers/editor/editor-action-validation.ts so each
 * family file declares only the properties its actions accept.
 */
import type { JsonObject } from '../../index.js';
import { str, num, bool } from '../shared/schema-props.js';

const int = (description: string): JsonObject => ({ type: 'integer', description });

export const P = {
  action: str('The control_editor action to execute.'),
  actorName: str('Name of the actor to target.'),
  name: str('Name identifier.'),
  objectPath: str('Object path alias for actorName.'),
  location: {
    type: 'object',
    properties: { x: num('x'), y: num('y'), z: num('z') },
    description: '3D location (x, y, z).',
    additionalProperties: false,
  },
  rotation: {
    type: 'object',
    properties: { pitch: num('pitch'), yaw: num('yaw'), roll: num('roll') },
    description: '3D rotation (pitch, yaw, roll).',
    additionalProperties: false,
  },
  blendTime: num('Blend time in seconds for set_view_target.'),
  speed: num('Game speed multiplier.'),
  deltaTime: num('Fixed delta time in seconds.'),
  fov: num('Camera field of view in degrees.'),
  viewMode: str('Viewport view mode (e.g. Lit, Unlit, Wireframe).'),
  width: num('Viewport width in pixels.'),
  height: num('Viewport height in pixels.'),
  mode: str('Editor mode for set_editor_mode, or screenshot source.'),
  enabled: bool('Whether the feature is enabled.'),
  realtime: bool('Whether realtime rendering is enabled.'),
  validateOnly: bool('Report whether a restart would proceed, and what it would discard, without restarting.'),
  discardUnsaved: bool('Restart even though packages have unsaved changes, discarding them. Omitted, a restart with unsaved work is refused.'),
  delaySeconds: num('Seconds to wait before restarting, so the response reaches the caller first. Clamped to 0.1-30.'),
  stat: str('Stat name to show or hide.'),
  command: str('Console command string.'),
  filename: str('Screenshot or recording filename.'),
  path: str('Directory or file path.'),
  resolution: str('Resolution setting (e.g. 1024x1024).'),
  returnBase64: bool('Return PNG image data as base64. Defaults to false — a plain capture returns path + metadata. Set true for inline image data; pair with resolution= (e.g. "1280x720") to keep the PNG under the base64 size cap.'),
  includeMetadata: bool('Attach caller-provided metadata to the response.'),
  metadata: {
    type: 'object',
    description: 'Caller-provided metadata.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  bookmarkName: str('Bookmark name identifier.'),
  id: str('Bookmark identifier.'),
  description: str('Bookmark description.'),
  assetPath: str('Asset path (e.g. /Game/Path/Asset).'),
  levelPath: str('Level asset path.'),
  category: str('Preferences category.'),
  preferences: {
    type: 'object',
    description: 'Editor preferences key-value pairs.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  steps: int('Number of frames to step.'),
  key: str('Input key name for simulate_input.'),
  type: str('Input event type (key_down, key_up, mouse_click, mouse_move), or widget_list / widget_click to operate the live UMG of a PIE session.'),
  inputType: str('Alias for type used by simulate_input.'),
  inputAction: str('Enhanced Input action to inject, as an asset path such as /Game/Input/IA_Move. Required for an Enhanced Input game: a raw key never reaches an InputAction, so plain key_down does nothing there.'),
  value: num('Scalar value to inject for inputAction (default 1; use a negative value for the opposite direction), or the value to set a Slider to with widget_click. Ignored for a raw key.'),
  holdSeconds: num('Keep injecting inputAction for this many seconds of GAME time so the pawn actually travels (default 0, a single frame). Game time, not wall time: under set_game_speed 0.05 a 2s hold still delivers 2s of in-game input, which takes 40s of real time. A key_up for the same action stops the hold early.'),
  x: num('Mouse X coordinate for simulate_input.'),
  y: num('Mouse Y coordinate for simulate_input.'),
  button: str('Mouse button for simulate_input.'),
  widget: str('For type widget_click: the live UMG widget to drive, by name (PlayButton), or Owner.Name (WBP_MainMenu.PlayButton) when several share it. widget_list shows both.'),
  frameRate: num('Recording frame rate.'),
  durationSeconds: num('Recording duration in seconds.'),
};
