// Stage 1b of the canonical execute pipeline: the lookup half of resolution.
//
// `gateway-execute-resolve.ts` owns the request-form types; this file turns a
// request into exactly one capability record, or into a typed refusal.
//
// Two request forms are accepted and neither wins by precedence:
//   v2      { capability, params, options }
//   legacy  { tool, action, params, options }   (generated from `legacyIds`)
// When both are supplied they must designate the same capability; disagreement
// is a FORM_CONFLICT rather than a silent pick. Aliases resolve visibly.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { legacyPairKey, type CapabilityIndex } from './gateway-capability-index.js';
import { closestMatches, buildNextCall, guideUnknownAction, MAX_SUGGESTIONS } from './gateway-guidance.js';
import type {
  ExecuteResolution,
  ExecuteResolutionFailure,
  LegacyPair
} from './gateway-execute-resolve.js';

export function primaryLegacyPair(record: CapabilityRecord): LegacyPair {
  const first = record.legacyIds[0];
  return first === undefined
    ? { tool: record.routing.parentTool, action: record.routing.dispatchAction }
    : { tool: first.tool, action: first.action };
}

function foldedPairForAlias(record: CapabilityRecord, alias: string | undefined): LegacyPair | undefined {
  if (alias === undefined) return undefined;
  const action = alias.slice(alias.lastIndexOf('.') + 1);
  const folded = record.legacyIds.find((legacy) => legacy.folded !== undefined && String(legacy.action) === action);
  return folded === undefined ? undefined : { tool: folded.tool, action: folded.action };
}

function fail(failure: ExecuteResolutionFailure): ExecuteResolution {
  return { ok: false, failure };
}

type CapabilityLookup =
  | { readonly kind: 'absent' }
  | { readonly kind: 'found'; readonly record: CapabilityRecord; readonly alias?: string }
  | { readonly kind: 'failed'; readonly failure: ExecuteResolutionFailure };

function lookupByCapability(capability: string | undefined, index: CapabilityIndex): CapabilityLookup {
  if (capability === undefined) return { kind: 'absent' };

  const canonical = index.byId.get(capability);
  if (canonical !== undefined) return { kind: 'found', record: canonical };

  // capabilityIndex() refuses to build over a contested alias or pair, so every
  // selector here has exactly one owner.
  const owned = index.byAlias.get(capability);
  if (owned !== undefined) return { kind: 'found', record: owned, alias: capability };

  const suggestions = closestMatches(capability, [...index.byId.keys()], MAX_SUGGESTIONS);
  return {
    kind: 'failed',
    failure: {
      errorCode: 'UNKNOWN_CAPABILITY',
      message: `Unknown capability '${capability}'. Call search before execute.`,
      suggestions,
      nextCall: suggestions[0] === undefined
        ? buildNextCall({ operation: 'search' })
        : { operation: 'describe', capability: suggestions[0] }
    }
  };
}

type LegacyLookup =
  | { readonly kind: 'absent' }
  | { readonly kind: 'found'; readonly record: CapabilityRecord; readonly pair: LegacyPair }
  | { readonly kind: 'failed'; readonly failure: ExecuteResolutionFailure };

function lookupByLegacyPair(
  requestedTool: string | undefined,
  action: string | undefined,
  index: CapabilityIndex
): LegacyLookup {
  if (requestedTool === undefined && action === undefined) return { kind: 'absent' };

  // A capability ID's namespace is not always a parent tool name, so the prefix
  // a caller reads off a search row resolves here before any lookup. Resolution
  // is second: a real tool name always wins over a namespace of the same text.
  const tool = requestedTool !== undefined && !index.actionsByParentTool.has(requestedTool)
    ? index.parentToolByNamespace.get(requestedTool) ?? requestedTool
    : requestedTool;

  if (tool === undefined || !index.actionsByParentTool.has(tool)) {
    const suggestions = closestMatches(tool ?? '', [...index.actionsByParentTool.keys()], MAX_SUGGESTIONS);
    return {
      kind: 'failed',
      failure: {
        errorCode: 'UNKNOWN_TOOL',
        message: 'Unknown tool. Call search before execute.',
        suggestions,
        nextCall: suggestions[0] === undefined
          ? buildNextCall({ operation: 'search' })
          : buildNextCall({ operation: 'describe', tool: suggestions[0] })
      }
    };
  }

  const record = action === undefined ? undefined : index.byLegacyPair.get(legacyPairKey(tool, action));
  if (record === undefined) {
    const available = index.actionsByParentTool.get(tool) ?? [];
    const owners = action === undefined
      ? []
      : [...index.actionsByParentTool].filter(([, actions]) => actions.includes(action)).map(([owner]) => owner).sort();
    const guide = guideUnknownAction(tool, action ?? '', available, owners);
    return {
      kind: 'failed',
      failure: {
        errorCode: 'UNKNOWN_ACTION',
        message: `Unknown action for ${tool}.${guide.hint} Call describe before execute.`,
        availableActions: available,
        suggestions: guide.suggestions,
        nextCall: guide.nextCall
      }
    };
  }

  return { kind: 'found', record, pair: { tool, action: action ?? '' } };
}

export function resolveExecuteTarget(
  request: {
    readonly capability?: string;
    readonly tool?: string;
    readonly action?: string;
    readonly params?: Record<string, unknown>;
  },
  index: CapabilityIndex
): ExecuteResolution {
  const fromCapability = lookupByCapability(request.capability, index);
  if (fromCapability.kind === 'failed') return fail(fromCapability.failure);

  const fromLegacy = lookupByLegacyPair(request.tool, request.action, index);
  if (fromLegacy.kind === 'failed') return fail(fromLegacy.failure);

  if (fromCapability.kind === 'found' && fromLegacy.kind === 'found'
    && fromCapability.record.id !== fromLegacy.record.id) {
    return fail({
      errorCode: 'FORM_CONFLICT',
      capabilityId: fromCapability.record.id,
      message: `capability '${fromCapability.record.id}' conflicts with tool/action '${fromLegacy.record.id}'. Supply one form.`,
      nextCall: { operation: 'describe', capability: fromCapability.record.id }
    });
  }

  const resolved = fromCapability.kind === 'found' ? fromCapability.record : undefined;
  const migrated = fromLegacy.kind === 'found' ? fromLegacy.record : undefined;
  const record = resolved ?? migrated;
  if (record === undefined) {
    return fail({
      errorCode: 'MISSING_SELECTOR',
      message: 'execute requires either capability or tool + action. Call describe with no arguments to list the parent tools, or search to find a capability.',
      nextCall: buildNextCall({ operation: 'describe' })
    });
  }

  // The pair this call came in as.
  const legacy = fromLegacy.kind === 'found'
    ? fromLegacy.pair
    : (fromCapability.kind === 'found' ? foldedPairForAlias(record, fromCapability.alias) : undefined) ?? primaryLegacyPair(record);
  return {
    ok: true,
    target: {
      record,
      legacy,
      ...(fromCapability.kind === 'found' && fromCapability.alias !== undefined
        ? { resolvedFromAlias: fromCapability.alias }
        : {}),
      ...(fromLegacy.kind === 'found' ? { migratedFrom: fromLegacy.pair } : {})
    }
  };
}
