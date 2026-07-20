/**
 * Shared JSON-schema property fragments for control_editor capability records.
 *
 * Mirrors the canonical control_editor tool definition parameter shapes from
 * src/tools/definitions/core/control-editor-tool.ts and the allowed-params
 * contract in src/tools/handlers/editor/editor-action-validation.ts so each
 * family file declares only the properties its actions accept.
 */
import type { JsonObject } from '../../index.js';

const str = (description: string): JsonObject => ({ type: 'string', description });
const num = (description: string): JsonObject => ({ type: 'number', description });
const int = (description: string): JsonObject => ({ type: 'integer', description });
const bool = (description: string): JsonObject => ({ type: 'boolean', description });

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
  stat: str('Stat name to show or hide.'),
  command: str('Console command string.'),
  filename: str('Screenshot or recording filename.'),
  path: str('Directory or file path.'),
  resolution: str('Resolution setting (e.g. 1024x1024).'),
  returnBase64: bool('Return PNG image data as base64.'),
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
  type: str('Input event type (key_down, key_up, mouse_click, mouse_move).'),
  inputType: str('Alias for type used by simulate_input.'),
  inputAction: str('Input action descriptor for simulate_input.'),
  x: num('Mouse X coordinate for simulate_input.'),
  y: num('Mouse Y coordinate for simulate_input.'),
  button: str('Mouse button for simulate_input.'),
  frameRate: num('Recording frame rate.'),
  durationSeconds: num('Recording duration in seconds.'),
};
