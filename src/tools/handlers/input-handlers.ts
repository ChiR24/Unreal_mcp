import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import type { HandlerArgs, InputArgs } from '../../types/handler-types.js';
import { InputTools } from '../input.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

export async function handleInputTools(
    action: string,
    args: HandlerArgs,
    tools: ITools
): Promise<Record<string, unknown>> {
    const argsTyped = args as InputArgs;
    const argsRecord = args as Record<string, unknown>;
    const inputTools = tools.inputTools as InputTools;
    if (!inputTools) {
        return ResponseFactory.error('Input tools not available');
    }

    const timeoutMs = getTimeoutMs();

    // All actions are dispatched to C++ via automation bridge
    const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
      const payload = { ...argsRecord, subAction };
      const result = await executeAutomationRequest(
        tools,
        'manage_input',
        payload as HandlerArgs,
        `Automation bridge not available for input action: ${subAction}`,
        { timeoutMs }
      );
      return cleanObject(result) as Record<string, unknown>;
    };

    switch (action) {
        case 'create_input_action':
            return cleanObject(await inputTools.createInputAction(argsTyped.name || '', argsTyped.path || '')) as Record<string, unknown>;
        case 'create_input_mapping_context':
            return cleanObject(await inputTools.createInputMappingContext(argsTyped.name || '', argsTyped.path || '')) as Record<string, unknown>;
        case 'add_mapping':
            return cleanObject(await inputTools.addMapping(argsTyped.contextPath ?? '', argsTyped.actionPath ?? '', argsTyped.key ?? '')) as Record<string, unknown>;
        case 'remove_mapping':
            return cleanObject(await inputTools.removeMapping(argsTyped.contextPath ?? '', argsTyped.actionPath ?? '')) as Record<string, unknown>;

        // New actions - dispatched to C++ via automation bridge
        case 'map_input_action':
            return sendRequest('map_input_action');

        case 'set_input_trigger':
            return sendRequest('set_input_trigger');

        case 'set_input_modifier':
            return sendRequest('set_input_modifier');

        case 'enable_input_mapping':
            return sendRequest('enable_input_mapping');

        case 'disable_input_action':
            return sendRequest('disable_input_action');

        case 'get_input_info':
            return sendRequest('get_input_info');

        default:
            return ResponseFactory.error(`Unknown input action: ${action}`);
    }
}
