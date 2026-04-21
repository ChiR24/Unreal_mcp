/**
 * Curve System Handlers (Ch5: manage_curve)
 *
 * Implements manage_curve actions covering:
 * - UCurveFloat create (create_curve_float)
 * - Keyframe read / write (get_curve_keys, set_curve_keys)
 * - Summary inspection (inspect_curve)
 *
 * Dispatches to C++ via the automation bridge using a "manage_curve" tool
 * action with a `subAction` payload field (consistent with manage_data).
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

async function sendCurveRequest(
  tools: ITools,
  subAction: string,
  payload: Record<string, unknown>
): Promise<Record<string, unknown>> {
  const timeoutMs = getTimeoutMs();
  const result = await executeAutomationRequest(
    tools,
    'manage_curve',
    { ...payload, subAction } as HandlerArgs,
    `Automation bridge not available for manage_curve action: ${subAction}`,
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

function requireArrayArg(args: Record<string, unknown>, field: string): unknown[] {
  const value = args[field];
  if (!Array.isArray(value)) {
    throw new Error(`Missing required parameter: ${field}`);
  }
  return value;
}

export async function handleCurveTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;

  switch (action) {
    case 'create_curve_float': {
      const path = requireStringArg(argsRecord, 'path');
      const name = requireStringArg(argsRecord, 'name');
      return sendCurveRequest(tools, 'create_curve_float', { path, name });
    }

    case 'set_curve_keys': {
      const path = requireStringArg(argsRecord, 'path');
      const keys = requireArrayArg(argsRecord, 'keys');
      return sendCurveRequest(tools, 'set_curve_keys', { path, keys });
    }

    case 'get_curve_keys': {
      const path = requireStringArg(argsRecord, 'path');
      return sendCurveRequest(tools, 'get_curve_keys', { path });
    }

    case 'inspect_curve': {
      const path = requireStringArg(argsRecord, 'path');
      return sendCurveRequest(tools, 'inspect_curve', { path });
    }

    default: {
      throw new Error(`Unsupported manage_curve action: ${action}`);
    }
  }
}
