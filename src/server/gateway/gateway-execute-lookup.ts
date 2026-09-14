// Stage 1b of the canonical execute pipeline: the lookup half of resolution.
//
// `gateway-execute-resolve.ts` owns the request-form types and the index built
// over them; this file turns a request into exactly one capability record, or
// into a typed refusal.
//
// Two request forms are accepted and neither wins by precedence:
//   v2      { capability, params, options }
//   legacy  { tool, action, params, options }   (generated from `legacyIds`)
// When both are supplied they must designate the same capability; disagreement
// is a FORM_CONFLICT rather than a silent pick. Aliases resolve visibly, and an
// alias owned by more than one capability is refused instead of guessed.
//
// Retirement is authoritative here too: a legacy pair the Task 20 migration map
// marks `removed`, or one a curated lossy rule refuses, never reaches dispatch.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { resolveMigrationEntry } from '../../tools/catalog/capabilities/migration/migration-map.js';
import { buildReplacementGuidance, findLossyRule } from '../../tools/catalog/capabilities/migration/lossy-translations.js';
import type { LegacyKey } from '../../tools/catalog/capabilities/migration/types.js';
import { legacyPairKey } from './gateway-capability-index.js';
import { closestMatches, buildNextCall, MAX_SUGGESTIONS } from './gateway-guidance.js';
import type {
  ExecuteResolution,
  ExecuteResolutionFailure,
  ExecuteTargetIndex,
  LegacyPair
} from './gateway-execute-resolve.js';

function primaryLegacyPair(record: CapabilityRecord): LegacyPair {
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

function lookupByCapability(capability: string | undefined, index: ExecuteTargetIndex): CapabilityLookup {
  if (capability === undefined) return { kind: 'absent' };

  const canonical = index.byId.get(capability);
  if (canonical !== undefined) return { kind: 'found', record: canonical };

  const owners = index.aliasOwners.get(capability) ?? [];
  if (owners.length > 1) {
    return {
      kind: 'failed',
      failure: {
        errorCode: 'ALIAS_CONFLICT',
        message: `Alias '${capability}' resolves to ${owners.length} capabilities: ${[...owners].sort().join(', ')}. Call execute with one of those canonical IDs.`,
        suggestions: [...owners].sort().slice(0, MAX_SUGGESTIONS)
      }
    };
  }

  const owned = owners[0] === undefined ? undefined : index.byId.get(owners[0]);
  if (owned !== undefined) return { kind: 'found', record: owned, alias: capability };

  const suggestions = closestMatches(capability, [...index.ids], MAX_SUGGESTIONS);
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
  index: ExecuteTargetIndex
): LegacyLookup {
  if (requestedTool === undefined && action === undefined) return { kind: 'absent' };

  // A capability ID's namespace is not always a parent tool name, so the prefix
  // a caller reads off a search row resolves here before any lookup. Resolution
  // is second: a real tool name always wins over a namespace of the same text.
  const tool = requestedTool !== undefined && !index.actionsByParentTool.has(requestedTool)
    ? index.parentToolByNamespace.get(requestedTool) ?? requestedTool
    : requestedTool;

  if (tool === undefined || !index.actionsByParentTool.has(tool)) {
    const suggestions = closestMatches(tool ?? '', [...index.parentTools], MAX_SUGGESTIONS);
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
    const suggestions = closestMatches(action ?? '', [...available], MAX_SUGGESTIONS);
    return {
      kind: 'failed',
      failure: {
        errorCode: 'UNKNOWN_ACTION',
        message: `Unknown action for ${tool}. Call describe before execute.`,
        availableActions: available,
        suggestions,
        nextCall: suggestions[0] === undefined
          ? buildNextCall({ operation: 'describe', tool })
          : buildNextCall({ operation: 'describe', tool, action: suggestions[0] })
      }
    };
  }

  // `byLegacyPair` keeps one winner per key, so a pair claimed by two records
  // would dispatch to whichever was indexed last. Refuse with both owners named
  // instead: the caller can re-issue against the canonical ID it meant.
  const owners = index.legacyPairOwners.get(legacyPairKey(tool, action ?? '')) ?? [];
  if (owners.length > 1) {
    const sorted = [...owners].sort();
    return {
      kind: 'failed',
      failure: {
        errorCode: 'LEGACY_PAIR_CONFLICT',
        message: `'${tool}.${action ?? ''}' resolves to ${sorted.length} capabilities: ${sorted.join(', ')}. Call execute with one of those canonical IDs.`,
        suggestions: sorted.slice(0, MAX_SUGGESTIONS),
        nextCall: { operation: 'describe', capability: sorted[0] }
      }
    };
  }

  return { kind: 'found', record, pair: { tool, action: action ?? '' } };
}

/** Refuse retired verbs and lossy legacy shapes before anything is validated. */
function checkMigration(
  record: CapabilityRecord,
  legacy: LegacyPair,
  params: Record<string, unknown>
): ExecuteResolutionFailure | undefined {
  const lossy = findLossyRule(`${legacy.tool}::${legacy.action}` as LegacyKey, params);
  if (lossy !== undefined) {
    const guidance = buildReplacementGuidance(lossy);
    return {
      errorCode: 'MIGRATION_NON_TRANSLATABLE',
      capabilityId: record.id,
      message: `${guidance.reason} Use ${guidance.nextCall.tool}.${guidance.nextCall.action} instead.`,
      nextCall: buildNextCall({
        operation: 'execute',
        tool: guidance.nextCall.tool,
        action: guidance.nextCall.action
      })
    };
  }

  const entry = resolveMigrationEntry(legacy.tool, legacy.action);
  if (entry?.disposition === 'removed') {
    return {
      errorCode: 'CAPABILITY_REMOVED',
      capabilityId: record.id,
      message: `Capability '${record.id}' was removed: ${entry.removal?.guidance ?? 'the legacy verb was retired.'}`,
      nextCall: buildNextCall({ operation: 'search' })
    };
  }

  return undefined;
}

export function resolveExecuteTarget(
  request: {
    readonly capability?: string;
    readonly tool?: string;
    readonly action?: string;
    readonly params?: Record<string, unknown>;
  },
  index: ExecuteTargetIndex
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

  // The pair this call came in as. A folded old name keeps its own migration
  // and lossy-rule outcome, so folding never revives a pair that was retired.
  const legacy = fromLegacy.kind === 'found'
    ? fromLegacy.pair
    : (fromCapability.kind === 'found' ? foldedPairForAlias(record, fromCapability.alias) : undefined) ?? primaryLegacyPair(record);
  const migrationFailure = checkMigration(record, legacy, request.params ?? {});
  if (migrationFailure !== undefined) return fail(migrationFailure);

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
