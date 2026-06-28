/**
 * Actor Layer Handlers
 *
 * Manage editor layers for actor organization:
 * - create_layer: Create a new editor layer
 * - delete_layer: Delete a layer
 * - rename_layer: Rename a layer
 * - list_layers: List all layers
 * - add_actor_to_layer: Add an actor to a layer
 * - remove_actor_from_layer: Remove an actor from a layer
 * - get_actor_layers: Get layers an actor belongs to
 * - set_layer_visibility: Toggle layer visibility
 * - get_layer_actors: Get all actors in a layer
 *
 * @module layer-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleLayerTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_layers',
    payload as HandlerArgs,
    `Automation bridge not available for layer action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
