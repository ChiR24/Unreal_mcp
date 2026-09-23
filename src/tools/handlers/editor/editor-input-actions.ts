import type { EditorArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { EDITOR_ACTION_UNHANDLED, editorActionHandled, type EditorActionResult } from './editor-action-result.js';

const INPUT_TYPE_ALIASES: Record<string, string> = {
  press: 'key_down',
  pressed: 'key_down',
  down: 'key_down',
  release: 'key_up',
  released: 'key_up',
  up: 'key_up',
  click: 'mouse_click',
  move: 'mouse_move'
};

const SUPPORTED_INPUT_TYPES = new Set(['key_down', 'key_up', 'mouse_click', 'mouse_move', 'widget_list', 'widget_click']);

function getInputType(args: EditorArgs): string {
  const inputTypeValue = args.type ?? args.inputType ?? args.inputAction;
  if (typeof inputTypeValue !== 'string' || inputTypeValue.trim() === '') {
    throw new Error('Missing required parameters for control_editor:simulate_input: [type|inputType|inputAction]');
  }

  const normalized = inputTypeValue.trim().toLowerCase();
  if ((normalized === 'key' || normalized === 'keyboard') && typeof args.inputAction === 'string') {
    const normalizedInputAction = args.inputAction.trim().toLowerCase();
    const mappedActionType = INPUT_TYPE_ALIASES[normalizedInputAction] ?? normalizedInputAction;
    if (mappedActionType === 'key_down' || mappedActionType === 'key_up') {
      return mappedActionType;
    }
  }

  const mappedType = INPUT_TYPE_ALIASES[normalized] ?? normalized;
  if (!SUPPORTED_INPUT_TYPES.has(mappedType)) {
    throw new Error(`Unknown input type: ${inputTypeValue}. Supported: ${[...SUPPORTED_INPUT_TYPES].join(', ')}`);
  }

  return mappedType;
}

export async function handleEditorInputAction(
  action: string,
  args: EditorArgs,
  tools: ITools
): Promise<EditorActionResult> {
  if (action !== 'simulate_input') {
    return EDITOR_ACTION_UNHANDLED;
  }

  const mappedType = getInputType(args);
  const res = await executeAutomationRequest(tools, 'control_editor', {
    action: 'simulate_input',
    type: mappedType,
    key: args.key,
    x: args.x,
    y: args.y,
    button: args.button,
    // The plugin reads these too: inputAction/value/holdSeconds drive Enhanced
    // Input, widget/value drive widget_click. Dropping them here left the stdio
    // path able to send only raw keys. An inputAction that is not an asset path
    // was a type alias ("pressed") and has already been consumed above.
    inputAction: args.inputAction?.startsWith('/') ? args.inputAction : undefined,
    value: args.value,
    holdSeconds: args.holdSeconds,
    widget: args.widget
  });
  return editorActionHandled(cleanObject(res));
}
