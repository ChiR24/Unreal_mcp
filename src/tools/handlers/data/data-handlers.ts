import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleDataTools(action: string, args: HandlerArgs, tools: ITools): Promise<any> {
  const payload = {
    ...args,
    subAction: action
  };
  
  const response = await executeAutomationRequest(
    tools,
    'manage_data',
    payload,
    `Automation bridge is not available for data action '${action}'.`
  );
  
  return cleanObject(response);
}
