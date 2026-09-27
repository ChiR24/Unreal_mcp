/**
 * Viewport display records: set_view_mode, set_viewport_resolution,
 * set_viewport_realtime, set_editor_mode, set_immersive_mode,
 * set_game_view, show_stats, hide_stats.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'viewport';
const D = 'editor';

export const VIEWPORT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_view_mode', domain: D, family: F,
    summary: 'Set the viewport rendering view mode (Lit, Unlit, Wireframe, etc.).',
    whenToUse: ['The viewport render mode must be changed for debugging or visualization.'],
    whenNotToUse: ['The default Lit mode is acceptable.'],
    inputProps: { viewMode: P.viewMode },
    required: ['viewMode'],
    effect: 'read',
   
    exampleInput: { action: 'set_view_mode', viewMode: 'Wireframe' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_viewport_resolution', dispatchAction: 'console_command',
    domain: D, family: F,
    summary: 'Set the viewport resolution via the r.SetRes console command.',
    whenToUse: ['The viewport resolution must be changed.'],
    whenNotToUse: ['The default resolution is acceptable.'],
    inputProps: { width: P.width, height: P.height },
    required: ['width', 'height'],
    effect: 'read',
   
    exampleInput: { action: 'set_viewport_resolution', width: 1920, height: 1080 },
    exampleOutput: { success: true, message: 'Resolution set', width: 1920, height: 1080 },
    outputProps: { width: P.width, height: P.height },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_viewport_realtime', domain: D, family: F,
    summary: 'Toggle realtime rendering in the editor viewport.',
    whenToUse: ['Realtime rendering must be enabled or disabled.'],
    whenNotToUse: ['The current realtime state is acceptable.'],
    inputProps: { enabled: P.enabled, realtime: P.realtime },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_viewport_realtime', enabled: false },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_editor_mode', domain: D, family: F,
    summary: 'Set the active editor mode (e.g. landscape, foliage, modeling).',
    whenToUse: ['A specific editor mode must be activated.'],
    whenNotToUse: ['The default mode is acceptable.'],
    inputProps: { mode: P.mode },
    required: ['mode'],
    effect: 'read',
   
    exampleInput: { action: 'set_editor_mode', mode: 'landscape' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_immersive_mode', domain: D, family: F,
    summary: 'Toggle immersive viewport mode.',
    whenToUse: ['The viewport must enter or exit immersive fullscreen.'],
    whenNotToUse: ['The current immersive state is acceptable.'],
    inputProps: { enabled: P.enabled },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_immersive_mode', enabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_game_view', domain: D, family: F,
    summary: 'Toggle game view in the editor viewport.',
    whenToUse: ['The viewport must show or hide the game view.'],
    whenNotToUse: ['The current game view state is acceptable.'],
    inputProps: { enabled: P.enabled },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'set_game_view', enabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'show_stats', domain: D, family: F,
    summary: 'Show a specific stat overlay in the viewport.',
    whenToUse: ['A debug stat must be displayed.'],
    whenNotToUse: ['The stat is already shown.'],
    inputProps: { stat: P.stat },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'show_stats', stat: 'fps' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'hide_stats', domain: D, family: F,
    summary: 'Hide a specific stat overlay in the viewport.',
    whenToUse: ['A debug stat must be hidden.'],
    whenNotToUse: ['The stat is already hidden.'],
    inputProps: { stat: P.stat },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'hide_stats', stat: 'fps' },
  }),
];
