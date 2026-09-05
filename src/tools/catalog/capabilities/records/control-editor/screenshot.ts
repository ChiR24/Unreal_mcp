/**
 * Screenshot records: screenshot, take_screenshot.
 *
 * Grounded in src/tools/handlers/editor/editor-screenshot-actions.ts.
 * take_screenshot aliases to screenshot. When mode is game_viewport the TS
 * handler cross-parent dispatches to system_control; other modes dispatch
 * to control_editor. Both are read-effect capture operations.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'screenshot';
const D = 'editor';

const SCREENSHOT_PROPS = {
  filename: P.filename,
  path: P.path,
  // Not P.resolution: this is a resample of one already-rendered frame, not a
  // re-render, so WxH is a bounding box rather than an exact output size --
  // aspect ratio is preserved and a box larger than the frame changes nothing.
  // It is also the documented way out of IMAGE_TOO_LARGE.
  resolution: {
    type: 'string',
    description: 'Maximum WxH for the returned PNG (e.g. "1280x720"). The capture is downscaled to fit inside this box with its aspect ratio preserved; a box at least as large as the viewport leaves the image untouched. Use this to bring an oversized capture under the base64 limit.'
  },
  mode: P.mode,
  returnBase64: P.returnBase64,
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
  mode: P.mode,
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
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'screenshot', mode: 'editor_viewport', filename: 'viewport' },
    exampleOutput: { success: true, screenshotPath: '/Game/Screenshots/viewport.png', mode: 'editor_viewport' },
    normalizationClass: 'A_TRUE_DUPLICATE',
    normalizationRationale: 'True duplicate shared across control_editor and system_control (cap:shared:screenshot); conditional cross-parent to system_control when mode is game_viewport.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'take_screenshot', dispatchAction: 'screenshot',
    domain: D, family: F,
    topics: ['screenshot', 'capture viewport', 'screen capture', 'viewport image', 'snapshot', 'take picture'],
    summary: 'Capture a screenshot (alias for screenshot).',
    whenToUse: ['A screenshot must be captured using the take_screenshot alias.'],
    whenNotToUse: ['The screenshot action is sufficient.'],
    inputProps: SCREENSHOT_PROPS,
    required: [],
    outputProps: SCREENSHOT_OUTPUT,
    effect: 'read',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'take_screenshot', mode: 'full_editor_window', filename: 'editor' },
    exampleOutput: { success: true, screenshotPath: '/Game/Screenshots/editor.png', mode: 'full_editor_window' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes take_screenshot to screenshot for handler routing; bridge dispatches screenshot. Conditional cross-parent to system_control when mode is game_viewport.',
  }),
];
