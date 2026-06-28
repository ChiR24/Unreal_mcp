/**
 * Gameplay Tag Handlers
 *
 * Manage the project's gameplay tag dictionary and actor tag assignments:
 * - add_tag: Register a new tag in the project dictionary
 * - remove_tag: Remove a tag from the dictionary
 * - list_tags: List all registered tags (optionally filtered by prefix)
 * - get_tag_children: Get direct children of a tag
 * - has_tag: Check if a tag exists
 * - add_tag_to_actor: Assign a gameplay tag to an actor
 * - remove_tag_from_actor: Remove a gameplay tag from an actor
 * - get_actor_tags: Get all gameplay tags on an actor
 * - get_tag_hierarchy: Get the full tag tree as nested JSON
 *
 * @module gameplay-tag-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleGameplayTagTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_gameplay_tags',
    payload as HandlerArgs,
    `Automation bridge not available for gameplay tag action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
