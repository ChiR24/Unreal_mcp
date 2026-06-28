/**
 * Physics Material Handlers
 *
 * Create and manage UPhysicalMaterial assets:
 * - create_physics_material: Create a new physical material
 * - set_physics_material_properties: Set friction, restitution, density, etc.
 * - get_physics_material_properties: Read physical material properties
 * - list_physics_materials: List all physical materials
 * - assign_physics_material: Assign a physical material to a mesh/component
 *
 * @module physics-material-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handlePhysicsMaterialTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_physics_material',
    payload as HandlerArgs,
    `Automation bridge not available for physics material action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
