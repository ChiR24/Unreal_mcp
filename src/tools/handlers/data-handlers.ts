/**
 * Data System Handlers (Ch2: DataTable + Ch3 DataAsset)
 *
 * Implements manage_data actions covering:
 * - UDataTable CRUD (create, add_row, set_row, update_row, remove_row)
 * - UDataTable reads (get_rows, list_rows)
 * - UDataTable schema migration (set_data_table_row_struct)
 * - UDataAsset create/get/set/list (Ch3)
 *
 * Dispatches to C++ via the automation bridge using a "manage_data" tool
 * action with a `subAction` payload field (consistent with other manage_*
 * tools like manage_inventory).
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

async function sendDataRequest(
  tools: ITools,
  subAction: string,
  payload: Record<string, unknown>
): Promise<Record<string, unknown>> {
  const timeoutMs = getTimeoutMs();
  const result = await executeAutomationRequest(
    tools,
    'manage_data',
    { ...payload, subAction } as HandlerArgs,
    `Automation bridge not available for manage_data action: ${subAction}`,
    { timeoutMs }
  );
  return cleanObject(result as Record<string, unknown>) as Record<string, unknown>;
}

function requireStringArg(args: Record<string, unknown>, field: string): string {
  const value = args[field];
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(`Missing required parameter: ${field}`);
  }
  return value;
}

function requireObjectArg(args: Record<string, unknown>, field: string): Record<string, unknown> {
  const value = args[field];
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`Missing required parameter: ${field}`);
  }
  return value as Record<string, unknown>;
}

export async function handleDataTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;

  // Tasks 2-9 (and Ch3) will add cases above this default. For now the skeleton
  // only exposes the action list via the TS schema; runtime calls with any
  // action throw "Unsupported". `tools` / helpers are referenced via the
  // default branch argument list below to keep the unused-lint clean.
  switch (action) {
    default: {
      // Ensure the helpers are referenced statically so tree-shakers and the
      // TypeScript noUnusedLocals checker don't drop them before Task 2 wires
      // them. They are invoked only via per-action cases in subsequent tasks.
      void tools; void argsRecord; void sendDataRequest;
      void requireStringArg; void requireObjectArg;
      throw new Error(`Unsupported manage_data action: ${action}`);
    }
  }
}
