import { cleanObject } from '../../utils/serialization/safe-json.js';
import { isRecord } from '../../utils/validation/type-guards.js';

type NormalizedToolCall = {
  name: string;
  action?: string;
  args: Record<string, unknown>;
};

export function normalizeToolCall(
  name: string,
  args: Record<string, unknown>
): NormalizedToolCall {
  const actionValue = args.action ?? args.subAction;
  const action = typeof actionValue === 'string' ? actionValue : undefined;

  return {
    name,
    action,
    args
  };
}

export function mergeActionParams(args: Record<string, unknown>): Record<string, unknown> {
  if (!isRecord(args.params)) return args;

  const merged = { ...args.params, ...args };
  delete merged.params;
  return merged;
}

export function addErrorContext(response: unknown, toolName: string, action: string | undefined): unknown {
  if (!isRecord(response)) return response;
  if (response.success !== false && response.isError !== true) return response;

  return cleanObject({
    ...response,
    isError: true,
    toolName: typeof response.toolName === 'string' ? response.toolName : toolName,
    action: typeof response.action === 'string' ? response.action : action ?? null
  });
}

export function classifyExecutionError(message: string): string {
  const lower = message.toLowerCase();
  if (lower.includes('timeout')) return 'TOOL_TIMEOUT';
  if (lower.includes('security violation')) return 'SECURITY_VIOLATION';
  return 'TOOL_EXECUTION_FAILED';
}
