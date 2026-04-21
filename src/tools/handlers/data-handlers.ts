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

  switch (action) {
    case 'create_data_table': {
      const path = requireStringArg(argsRecord, 'path');
      const name = requireStringArg(argsRecord, 'name');
      const rowStructPath = requireStringArg(argsRecord, 'rowStructPath');
      return sendDataRequest(tools, 'create_data_table', { path, name, rowStructPath });
    }

    case 'add_data_table_row': {
      const path = requireStringArg(argsRecord, 'path');
      const rowName = requireStringArg(argsRecord, 'rowName');
      const rawFields = argsRecord.fields;
      const fields = (rawFields && typeof rawFields === 'object' && !Array.isArray(rawFields))
        ? rawFields as Record<string, unknown>
        : {};
      return sendDataRequest(tools, 'add_data_table_row', { path, rowName, fields });
    }

    case 'set_data_table_row': {
      const path = requireStringArg(argsRecord, 'path');
      const rowName = requireStringArg(argsRecord, 'rowName');
      const fields = requireObjectArg(argsRecord, 'fields');
      return sendDataRequest(tools, 'set_data_table_row', { path, rowName, fields });
    }

    case 'update_data_table_row': {
      const path = requireStringArg(argsRecord, 'path');
      const rowName = requireStringArg(argsRecord, 'rowName');
      const fields = requireObjectArg(argsRecord, 'fields');
      return sendDataRequest(tools, 'update_data_table_row', { path, rowName, fields });
    }

    case 'remove_data_table_row': {
      const path = requireStringArg(argsRecord, 'path');
      const rowName = requireStringArg(argsRecord, 'rowName');
      return sendDataRequest(tools, 'remove_data_table_row', { path, rowName });
    }

    case 'get_data_table_rows': {
      const path = requireStringArg(argsRecord, 'path');
      const rawRowNames = argsRecord.rowNames;
      const payload: Record<string, unknown> = { path };
      if (Array.isArray(rawRowNames)) {
        payload.rowNames = rawRowNames.filter((v): v is string => typeof v === 'string');
      }
      return sendDataRequest(tools, 'get_data_table_rows', payload);
    }

    case 'list_data_table_rows': {
      const path = requireStringArg(argsRecord, 'path');
      return sendDataRequest(tools, 'list_data_table_rows', { path });
    }

    case 'set_data_table_row_struct': {
      const path = requireStringArg(argsRecord, 'path');
      const newRowStructPath = requireStringArg(argsRecord, 'newRowStructPath');
      return sendDataRequest(tools, 'set_data_table_row_struct', { path, newRowStructPath });
    }

    case 'create_data_asset': {
      const path = requireStringArg(argsRecord, 'path');
      const name = requireStringArg(argsRecord, 'name');
      const dataAssetClassPath = requireStringArg(argsRecord, 'dataAssetClassPath');
      return sendDataRequest(tools, 'create_data_asset', { path, name, dataAssetClassPath });
    }

    case 'set_data_asset_property': {
      const path = requireStringArg(argsRecord, 'path');
      const propertyPath = requireStringArg(argsRecord, 'propertyPath');
      if (argsRecord.value === undefined) {
        throw new Error('Missing required parameter: value');
      }
      return sendDataRequest(tools, 'set_data_asset_property', {
        path, propertyPath, value: argsRecord.value,
      });
    }

    case 'get_data_asset_property': {
      const path = requireStringArg(argsRecord, 'path');
      const propertyPath = requireStringArg(argsRecord, 'propertyPath');
      return sendDataRequest(tools, 'get_data_asset_property', { path, propertyPath });
    }

    default: {
      void requireObjectArg;
      throw new Error(`Unsupported manage_data action: ${action}`);
    }
  }
}
