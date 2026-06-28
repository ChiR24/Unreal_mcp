/**
 * Blueprint Interface Handlers
 *
 * Create and manage Blueprint Interfaces:
 * - create_blueprint_interface: Create a new Blueprint Interface asset
 * - add_function: Add a function signature to an interface
 * - remove_function: Remove a function from an interface
 * - list_functions: List all functions in an interface
 * - implement_interface: Add an interface to a Blueprint class
 * - remove_interface: Remove an interface from a Blueprint class
 * - list_interfaces: List interfaces implemented by a Blueprint
 *
 * @module blueprint-interface-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleBlueprintInterfaceTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_blueprint_interface',
    payload as HandlerArgs,
    `Automation bridge not available for blueprint interface action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
