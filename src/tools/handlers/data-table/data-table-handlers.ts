/**
 * Data Table Handlers
 *
 * Create, read, edit, and manage UDataTable assets:
 * - create_data_table: Create a new data table from a struct type
 * - list_rows: List all row names
 * - get_row: Get a row's column values as JSON
 * - add_row: Add a new row
 * - edit_row: Edit an existing row
 * - remove_row: Remove a row
 * - get_structure: Get column names and types
 * - import_json: Import rows from JSON
 * - export_json: Export all rows as JSON
 *
 * @module data-table-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleDataTableTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_data_table',
    payload as HandlerArgs,
    `Automation bridge not available for data table action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
