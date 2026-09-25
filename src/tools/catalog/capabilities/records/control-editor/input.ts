/**
 * Input simulation record: simulate_input.
 *
 * Grounded in src/tools/handlers/editor/editor-input-actions.ts.
 * TS normalizes the type field (press/release/click/move aliases) to one of
 * key_down, key_up, key_tap, mouse_click, mouse_move before dispatching to the bridge.
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
    // The widget verbs are not in the action name, and topics cannot outrank a
    // widget-authoring record that carries them: "click widget button" landed on
    // add_content_widget, which edits a Widget Blueprint instead of pressing one.
    aliases: ['control_editor.click_widget', 'control_editor.press_ui_button'],
    topics: ['click button', 'click ui button', 'click button in running game', 'press key', 'simulate key press', 'drive game ui', 'test running game'],
    summary: 'Simulate a keyboard or mouse input event (key_down, key_up, key_tap, mouse_click, mouse_move), or list and press the live UMG widgets of a PIE session (widget_list, widget_click).',
    whenToUse: [
      'Synthetic input must be injected into the editor or PIE.',
      'A game has to be played: key_tap taps a key, key_down with holdSeconds holds it for that many game seconds, and inputAction injects an Enhanced Input action directly when no key is mapped to it.',
      'A game UI must be operated in PIE: widget_list names every live widget, and widget_click presses a Button, toggles a CheckBox or sets a Slider (value) by name without touching the OS cursor, so it works while the editor window is in the background.',
    ],
    whenNotToUse: ['Real hardware input is available.'],
    inputProps: {
      key: P.key,
      type: P.type,
      inputType: P.inputType,
      inputAction: P.inputAction,
      value: P.value,
      holdSeconds: P.holdSeconds,
      x: P.x,
      y: P.y,
      button: P.button,
      widget: P.widget,
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
      handledByPIE: { type: 'boolean', description: 'PIE actually consumed the event. False here with routedToPIE true means the key reached the game and nothing bound it: no active input mapping context maps that key (inject the action with inputAction instead).' },
      handledBySlate: { type: 'boolean', description: 'Slate consumed the event (editor-level input).' },
      injectedAction: { type: 'string', description: 'The Enhanced Input action that was injected, when inputAction resolved to one. Absent means the call went down the raw-key path, which reaches the game only through a mapping context that maps the key.' },
      widgets: {
        type: 'array',
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        description: 'widget_list: each live user widget (userWidget, object, inViewport) with its Button/CheckBox/Slider children (name, type, enabled, visible, focused, and text, value or checked).',
      },
      widget: { type: 'string', description: 'widget_click: the widget that was driven, as Owner.Name.' },
      widgetType: { type: 'string', description: 'widget_click: the driven widget\'s class (Button, CheckBox, Slider).' },
    },
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'simulate_input', type: 'key_down', key: 'SpaceBar' },
    exampleOutput: { success: true, message: 'Input simulated', routedToPIE: true, handledByPIE: true, handledBySlate: false },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes input type aliases (press/release/key/tap/click/move) to key_down/key_up/key_tap/mouse_click/mouse_move before bridge dispatch. Distinct input verb.',
  }),
];
