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
  resolution: P.resolution,
  mode: P.mode,
  returnBase64: P.returnBase64,
  includeMetadata: P.includeMetadata,
  metadata: P.metadata,
};

const SCREENSHOT_OUTPUT = {
  imageBase64: { type: 'string', description: 'Base64-encoded PNG image data.' },
  mimeType: { type: 'string', description: 'Image MIME type.' },
  width: P.width,
  height: P.height,
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
