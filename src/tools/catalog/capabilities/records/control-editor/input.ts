/**
 * Input simulation record: simulate_input.
 *
 * Grounded in src/tools/handlers/editor/editor-input-actions.ts.
 * TS normalizes the type field (press/release/click/move aliases) to one of
 * key_down, key_up, mouse_click, mouse_move before dispatching to the bridge.
 * Write-effect: injects synthetic input events into the editor or PIE.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'input';
const D = 'editor';

export const INPUT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'simulate_input', domain: D, family: F,
    summary: 'Simulate a keyboard or mouse input event (key_down, key_up, mouse_click, mouse_move).',
    whenToUse: ['Synthetic input must be injected into the editor or PIE.'],
    whenNotToUse: ['Real hardware input is available.'],
    inputProps: {
      key: P.key,
      type: P.type,
      inputType: P.inputType,
      inputAction: P.inputAction,
      x: P.x,
      y: P.y,
      button: P.button,
    },
    required: [],
    // The handler already computes all three of these
    // (Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorInput.cpp),
    // but the record declared no outputs at all, so the gateway stripped them and
    // left only "delivered to PIE" -- which means ROUTED, not consumed. An
    // Enhanced Input game silently ignores a raw Slate key, and with these fields
    // dropped the call looked like an unqualified success while the pawn never
    // moved. handledByPIE is the field that distinguishes the two.
    outputProps: {
      routedToPIE: { type: 'boolean', description: 'The event was routed to the PIE viewport rather than the editor.' },
      handledByPIE: { type: 'boolean', description: 'PIE actually consumed the event. False here with routedToPIE true means the key reached the game and nothing bound it — the usual cause is an Enhanced Input game, where a raw key never reaches an InputAction.' },
      handledBySlate: { type: 'boolean', description: 'Slate consumed the event (editor-level input).' },
    },
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'simulate_input', type: 'key_down', key: 'SpaceBar' },
    exampleOutput: { success: true, message: 'Input simulated', routedToPIE: true, handledByPIE: true, handledBySlate: false },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes input type aliases (press/release/click/move) to key_down/key_up/mouse_click/mouse_move before bridge dispatch. Distinct input verb.',
  }),
];
