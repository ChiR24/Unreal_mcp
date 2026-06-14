import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';

export async function handleDataTools(action: string, args: HandlerArgs, tools: ITools): Promise<any> {
  const payload = {
    ...args,
    subAction: action
  };

  const argsRecord = args as Record<string, any>;
  if (action === 'read_config_value' || action === 'write_config_value' || action === 'flush_config') {
    requireNonEmptyString(argsRecord.configFilename, 'configFilename', 'Missing required parameter: configFilename');
    if (action !== 'flush_config') {
      requireNonEmptyString(argsRecord.configSection, 'configSection', 'Missing required parameter: configSection');
      requireNonEmptyString(argsRecord.configKey, 'configKey', 'Missing required parameter: configKey');
      if (action === 'write_config_value') {
        requireNonEmptyString(argsRecord.configValue, 'configValue', 'Missing required parameter: configValue');
      }
    }
  } else if (action === 'check_save_slot_exists' || action === 'delete_save_slot') {
    requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
  } else if (action === 'create_gameplay_tag') {
    requireNonEmptyString(argsRecord.tagName, 'tagName', 'Missing required parameter: tagName');
  }
  
  const response = await executeAutomationRequest(
    tools,
    'manage_data',
    payload,
    `Automation bridge is not available for data action '${action}'.`
  );
  
  return cleanObject(response);
}
