// src/server/gateway/gateway-describe.ts
// Progressive gateway discovery for the `unreal` tool, routed by selector.
//
// Levels, none of which dumps the catalog:
//   describe {}                          -> catalog domains
//   describe { domain }                  -> families in that domain
//   describe { domain?, family }         -> capabilities in that family
//   describe { capability }              -> ONE capability's exact contract
//   describe { capability, param }       -> ONE declared parameter's schema
//   describe { tool }                    -> legacy parent summary (migration view)
//   describe { tool, action }            -> the canonical capability behind that pair
//
// The legacy `{ tool }` summary is the only level still sourced from the parent
// manifest: it describes the legacy dispatch surface itself. Every capability
// level reads the generated canonical registry, so an action's parameters are
// exactly the ones it declares and never the parent tool's union.

import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import type { ToolDefinition } from '../../tools/definitions/shared/tool-definition.js';
import { resolveLegacyPair } from './gateway-capability-index.js';
import {
  describeCapabilityParameter,
  describeCapabilityRecord,
  describeCapabilityReference
} from './gateway-describe-capability.js';
import {
  DEFAULT_BROWSE_LIMIT,
  describeCatalog,
  describeDomain,
  describeFamily,
  resolveDomainForFamily,
  unknownFamilyError,
  type Page
} from './gateway-describe-browse.js';
import {
  getString,
  getBoundedInteger,
  getOptionalBoundedInteger,
  gatewayError,
  findTool,
  allToolNames,
  getActionValues
} from './gateway-shared.js';
import { closestMatches, buildNextCall, MAX_SUGGESTIONS } from './gateway-guidance.js';

const MAX_DESCRIBE_LIMIT = 50;

function unknownToolError(toolArg: string | undefined): Record<string, unknown> {
  const error = gatewayError('describe', 'UNKNOWN_TOOL', 'Unknown tool. Call search to retrieve canonical tool names.');
  const suggestions = closestMatches(toolArg ?? '', allToolNames(), MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? buildNextCall({ operation: 'describe', tool: suggestions[0] })
    : buildNextCall({ operation: 'search' });
  return { ...error, suggestions, nextCall };
}

function unknownActionError(toolName: string, actionArg: string, actions: string[]): Record<string, unknown> {
  const error = gatewayError('describe', 'UNKNOWN_ACTION', `Unknown action '${actionArg}' for ${toolName}.`);
  const suggestions = closestMatches(actionArg, actions, MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? buildNextCall({ operation: 'describe', tool: toolName, action: suggestions[0] })
    : buildNextCall({ operation: 'describe', tool: toolName });
  return { ...error, tool: toolName, availableActions: actions, suggestions, nextCall };
}

// Actions are bare names, so the whole list is returned unless the caller asks
// for a page. The gateway's own schema caps `limit` at 25 while parent tools
// carry up to 158 actions, so a default page size would make the full action
// set of a large tool unreachable through the public contract. Progressive
// disclosure still holds: no inputSchema is emitted and perActionSchemas stays
// false, because a parent tool has no single schema to report.
function describeToolSummary(
  tool: ToolDefinition,
  query: string,
  requestedLimit: number | undefined,
  offset: number
): Record<string, unknown> {
  const actions = getActionValues(tool);
  const filtered = query.length === 0
    ? actions
    : actions.filter((name) => name.toLowerCase().includes(query));
  const limit = requestedLimit ?? filtered.length;
  const paged = filtered.slice(offset, offset + limit);
  const first = paged[0] ?? actions[0];
  return {
    success: true,
    operation: 'describe',
    tool: tool.name,
    category: tool.category,
    description: tool.description,
    enabled: dynamicToolManager.isToolEnabled(tool.name),
    actions: paged,
    actionCount: filtered.length,
    actionOffset: offset,
    actionLimit: limit,
    actionHasMore: offset + paged.length < filtered.length,
    scope: 'tool',
    perActionSchemas: false,
    drillDown: buildNextCall({ operation: 'describe', tool: tool.name, action: first }),
    message: 'Legacy parent-tool summary. Drill into an action to get that action\'s exact capability contract.'
  };
}

function describeLegacyPair(
  toolName: string,
  action: string,
  param: string | undefined
): Record<string, unknown> {
  const resolved = resolveLegacyPair(toolName, action);
  if (resolved.kind === 'unknown') {
    const tool = findTool(toolName);
    return tool === undefined
      ? unknownToolError(toolName)
      : unknownActionError(tool.name, action, getActionValues(tool));
  }
  const origin = { migratedFrom: { tool: toolName, action } };
  return param === undefined
    ? describeCapabilityRecord(resolved.record, origin)
    : describeCapabilityParameter(resolved.record, param, origin);
}

function describeLegacyTool(
  toolName: string,
  args: Record<string, unknown>,
  page: Page,
  query: string
): Record<string, unknown> {
  const action = getString(args, 'action');
  if (action !== undefined) return describeLegacyPair(toolName, action, getString(args, 'param'));
  const tool = findTool(toolName);
  if (tool === undefined) return unknownToolError(toolName);
  const requestedLimit = getOptionalBoundedInteger(args.limit, 1, MAX_DESCRIBE_LIMIT);
  return describeToolSummary(tool, query, requestedLimit, page.offset);
}

function describeBrowse(
  domain: string | undefined,
  family: string | undefined,
  page: Page
): Record<string, unknown> {
  if (family !== undefined) {
    const owningDomain = domain ?? resolveDomainForFamily(family);
    if (owningDomain === undefined) return unknownFamilyError(domain ?? '', family);
    return describeFamily(owningDomain, family, page);
  }
  if (domain !== undefined) return describeDomain(domain, page);
  return describeCatalog(page);
}

function paramWithoutCapabilityError(toolArg: string | undefined): Record<string, unknown> {
  const error = gatewayError(
    'describe',
    'MISSING_ACTION',
    'param selects one parameter of one capability, so it needs a capability, or a tool with an action.'
  );
  const nextCall = toolArg === undefined
    ? buildNextCall({ operation: 'search' })
    : buildNextCall({ operation: 'describe', tool: toolArg });
  return { ...error, nextCall };
}

export function describeGatewayCapability(args: Record<string, unknown>): Record<string, unknown> {
  const query = (getString(args, 'query') ?? '').toLowerCase();
  const offset = getBoundedInteger(args.offset, 0, 0, Number.MAX_SAFE_INTEGER);
  const limit = getBoundedInteger(args.limit, DEFAULT_BROWSE_LIMIT, 1, MAX_DESCRIBE_LIMIT);
  const page: Page = { limit, offset };

  const capability = getString(args, 'capability');
  if (capability !== undefined) return describeCapabilityReference(capability, getString(args, 'param'));

  const toolArg = getString(args, 'tool');
  // Every level below this point ignores `param`, so answering them would return
  // a broader view than the caller asked for while looking like a success.
  if (getString(args, 'param') !== undefined && getString(args, 'action') === undefined) {
    return paramWithoutCapabilityError(toolArg);
  }
  if (toolArg !== undefined) return describeLegacyTool(toolArg, args, page, query);

  return describeBrowse(getString(args, 'domain'), getString(args, 'family'), page);
}
