import type { EditorArgs } from '../../../types/handlers/handler-types.js';
import {
  normalizePathFields,
  validateArgsSecurity,
  validateExpectedParams,
  validateRequiredParams
} from '../foundation/dispatch/common-handlers.js';

const EDITOR_ACTION_ALIASES: Record<string, string> = {
  focus_actor: 'focus',
  set_game_view_target: 'set_view_target',
  set_camera_position: 'set_camera',
  set_viewport_camera: 'set_camera',
  take_screenshot: 'screenshot',
  single_frame_step: 'step_frame',
};

const IDEMPOTENT_ACTIONS = new Set([
  'stop', 'stop_pie', 'pause', 'resume',
  'set_game_speed', 'set_fixed_delta_time',
  'set_immersive_mode', 'set_game_view',
  'show_stats', 'hide_stats',
  'undo', 'redo',
  'step_frame', 'single_frame_step'
]);

const ACTION_REQUIRED_PARAMS: Record<string, string[]> = {
  focus_actor: ['actorName'],
  focus: ['actorName'],
  possess: ['actorName'],
  set_camera: ['location', 'rotation'],
  set_viewport_resolution: ['width', 'height'],
  set_view_mode: ['viewMode'],
  set_editor_mode: ['mode'],
  set_camera_fov: ['fov'],
  set_game_speed: ['speed'],
  set_fixed_delta_time: ['deltaTime'],
  set_preferences: ['category', 'preferences'],
  execute_command: ['command'],
  console_command: ['command']
};

const ACTION_ALLOWED_PARAMS: Record<string, string[]> = {
  play: [],
  stop: [],
  stop_pie: [],
  pause: [],
  resume: [],
  eject: [],
  possess: ['actorName'],
  set_view_target: ['actorName', 'name', 'objectPath', 'location', 'rotation', 'blendTime'],
  open_asset: ['assetPath', 'path'],
  close_asset: ['assetPath', 'path'],
  open_level: ['levelPath', 'path', 'assetPath'],
  focus_actor: ['actorName', 'name'],
  focus: ['actorName', 'name'],
  set_camera: ['location', 'rotation', 'actorName'],
  set_viewport_resolution: ['width', 'height'],
  set_view_mode: ['viewMode'],
  set_editor_mode: ['mode'],
  set_camera_fov: ['fov'],
  set_game_speed: ['speed'],
  set_fixed_delta_time: ['deltaTime'],
  screenshot: ['filename', 'path', 'resolution', 'mode', 'returnBase64', 'includeMetadata', 'metadata'],
  set_preferences: ['category', 'preferences'],
  execute_command: ['command'],
  console_command: ['command'],
  undo: [],
  redo: [],
  save_all: [],
  show_stats: ['stat'],
  hide_stats: ['stat'],
  set_game_view: ['enabled'],
  set_immersive_mode: ['enabled'],
  step_frame: ['steps'],
  single_frame_step: ['steps'],
  create_bookmark: ['id', 'description', 'bookmarkName'],
  jump_to_bookmark: ['id', 'bookmarkName'],
  start_recording: ['filename', 'name', 'frameRate', 'durationSeconds', 'metadata'],
  stop_recording: [],
  set_viewport_realtime: ['enabled', 'realtime'],
  simulate_input: ['key', 'type', 'inputType', 'inputAction', 'x', 'y', 'button']
};

const EDITOR_ASSET_PATH_ACTIONS = new Set(['open_asset', 'close_asset', 'open_level']);
const EDITOR_PATH_FIELDS = ['assetPath', 'levelPath', 'path'] as const;

export type PreparedEditorAction = {
  readonly normalizedAction: string;
  readonly args: EditorArgs;
};

function normalizeEditorAction(action: string): string {
  return EDITOR_ACTION_ALIASES[action] ?? action;
}

function validateEditorActionArgs(action: string, args: Record<string, unknown>): void {
  validateArgsSecurity({ action, ...args });

  const requiredParams = ACTION_REQUIRED_PARAMS[action];
  if (requiredParams !== undefined) {
    validateRequiredParams(args, requiredParams, `control_editor:${action}`);
  }

  if (IDEMPOTENT_ACTIONS.has(action)) {
    return;
  }

  const allowedParams = ACTION_ALLOWED_PARAMS[action];
  if (allowedParams !== undefined) {
    validateExpectedParams(args, allowedParams, `control_editor:${action}`);
  }
}

export function prepareEditorAction(action: string, args: EditorArgs): PreparedEditorAction {
  const normalizedAction = normalizeEditorAction(action);
  const argsRecord = EDITOR_ASSET_PATH_ACTIONS.has(normalizedAction)
    ? normalizePathFields(args, EDITOR_PATH_FIELDS)
    : args;

  validateEditorActionArgs(normalizedAction, argsRecord);
  return { normalizedAction, args: argsRecord as EditorArgs };
}
