// src/server/gateway/direct-call-migration.ts
// Task 30: the public surface exposes only the `unreal` gateway tool. A client
// that still calls a canonical parent tool by name gets this bounded,
// copy-paste-executable migration receipt instead of a routed call. Wave 3 wires
// the receipt into the tool-call response; this module only computes guidance.

import { isRecord } from '../../utils/validation/type-guards.js';
import { closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { allToolNames, getString } from './gateway-shared.js';

const DIRECT_TOOL_CALL_REMOVED = 'DIRECT_TOOL_CALL_REMOVED';

// Dispatch selectors, the gateway `operation` key, and the nested-params wrapper
// are control fields, not action parameters, so the migrated execute params carry
// only the real parameters (native McpBuildDirectCallMigration strips the same set).
const CONTROL_FIELDS = ['action', 'subAction', 'params', 'operation'] as const;

type ExecuteNextCall = {
  readonly operation: 'execute';
  readonly tool: string;
  readonly action: string;
  readonly params: Record<string, unknown>;
};
type DescribeNextCall = { readonly operation: 'describe'; readonly tool: string };
type SearchNextCall = { readonly operation: 'search' };

// success:false and a top-level operation are the two fields the `unreal` output
// schema requires; wrapResponse promotes success:false to isError.
export type DirectCallMigrationResult = {
  readonly success: false;
  readonly operation: 'search' | 'describe' | 'execute';
  readonly errorCode: typeof DIRECT_TOOL_CALL_REMOVED;
  readonly tool: string;
  readonly message: string;
  readonly nextCall: ExecuteNextCall | DescribeNextCall | SearchNextCall;
  readonly suggestions?: readonly string[];
};

// Flatten a legacy call's nested `params` into the action parameters exactly as
// legacy dispatch did (top-level keys win), then drop the control fields, all on
// a fresh object so the caller's input is never mutated.
function migratedParams(args: Record<string, unknown>): Record<string, unknown> {
  const nested = isRecord(args.params) ? args.params : {};
  const merged: Record<string, unknown> = { ...nested, ...args };
  for (const field of CONTROL_FIELDS) delete merged[field];
  return merged;
}

export function buildDirectCallMigration(
  toolName: string,
  args: Record<string, unknown>
): DirectCallMigrationResult {
  const parents = allToolNames();

  if (!parents.includes(toolName)) {
    return {
      success: false,
      operation: 'search',
      errorCode: DIRECT_TOOL_CALL_REMOVED,
      tool: toolName,
      message: `Direct tool calls are removed. '${toolName}' is not a known tool; call the 'unreal' gateway with operation 'search' to discover the right capability.`,
      nextCall: { operation: 'search' },
      suggestions: closestMatches(toolName, parents, MAX_SUGGESTIONS)
    };
  }

  const action = getString(args, 'action') ?? getString(args, 'subAction');
  if (action === undefined) {
    return {
      success: false,
      operation: 'describe',
      errorCode: DIRECT_TOOL_CALL_REMOVED,
      tool: toolName,
      message: `Direct tool calls are removed. Call the 'unreal' gateway: describe '${toolName}' to choose an action, then execute it.`,
      nextCall: { operation: 'describe', tool: toolName }
    };
  }

  return {
    success: false,
    operation: 'execute',
    errorCode: DIRECT_TOOL_CALL_REMOVED,
    tool: toolName,
    message: `Direct tool calls are removed. Call the 'unreal' gateway with operation 'execute' to run '${toolName}.${action}'.`,
    nextCall: { operation: 'execute', tool: toolName, action, params: migratedParams(args) }
  };
}
