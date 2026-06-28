/**
 * String Table Handlers
 *
 * Manage FStringTable assets for localization and UI text:
 * - create_string_table: Create a new string table asset
 * - add_entry: Add a key-value entry to a string table
 * - remove_entry: Remove an entry by key
 * - edit_entry: Edit an existing entry's value
 * - get_entry: Get a single entry by key
 * - list_entries: List all entries in a string table
 * - import_json: Import entries from JSON
 * - export_json: Export all entries as JSON
 * - list_string_tables: List all string table assets
 *
 * @module string-table-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleStringTableTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_string_table',
    payload as HandlerArgs,
    `Automation bridge not available for string table action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
