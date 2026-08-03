// src/server/gateway/gateway-execute-resolve.ts
// Stage 1 of the canonical execute pipeline: turn a request form into exactly
// one capability record, or into a typed refusal.
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
import { capabilityIndex, deriveNamespaceAliases, legacyPairKey } from './gateway-capability-index.js';
import { closestMatches, buildNextCall, MAX_SUGGESTIONS } from './gateway-guidance.js';

export type LegacyPair = { readonly tool: string; readonly action: string };

export type ExecuteTargetIndex = {
  readonly ids: readonly string[];
  readonly parentTools: readonly string[];
  readonly byId: ReadonlyMap<string, CapabilityRecord>;
  /** Alias -> every capability that declares it, so a collision is visible. */
  readonly aliasOwners: ReadonlyMap<string, readonly string[]>;
  readonly byLegacyPair: ReadonlyMap<string, CapabilityRecord>;
  readonly actionsByParentTool: ReadonlyMap<string, readonly string[]>;
  /** Capability ID namespace -> the parent tool that dispatches it. */
  readonly parentToolByNamespace: ReadonlyMap<string, string>;
};

export type ExecuteTarget = {
  readonly record: CapabilityRecord;
  readonly legacy: LegacyPair;
  readonly resolvedFromAlias?: string;
  readonly migratedFrom?: LegacyPair;
};

export type ExecuteResolutionFailure = {
  readonly errorCode: string;
  readonly message: string;
  readonly capabilityId?: string;
  readonly suggestions?: readonly string[];
  readonly nextCall?: Record<string, unknown>;
  readonly availableActions?: readonly string[];
};

export type ExecuteResolution =
  | { readonly ok: true; readonly target: ExecuteTarget }
  | { readonly ok: false; readonly failure: ExecuteResolutionFailure };

export function buildExecuteTargetIndex(records: readonly CapabilityRecord[]): ExecuteTargetIndex {
  const byId = new Map<string, CapabilityRecord>();
  const aliasOwners = new Map<string, string[]>();
  const byLegacyPair = new Map<string, CapabilityRecord>();
  const actionsByParentTool = new Map<string, string[]>();

  for (const record of records) {
    byId.set(record.id, record);
    for (const alias of record.aliases) {
      const owners = aliasOwners.get(alias) ?? [];
      owners.push(record.id);
      aliasOwners.set(alias, owners);
    }
    for (const legacy of record.legacyIds) {
      byLegacyPair.set(legacyPairKey(legacy.tool, legacy.action), record);
      const actions = actionsByParentTool.get(legacy.tool) ?? [];
      actions.push(legacy.action);
      actionsByParentTool.set(legacy.tool, actions);
    }
  }

  return {
    ids: [...byId.keys()],
    parentTools: [...actionsByParentTool.keys()],
    byId,
    aliasOwners,
    byLegacyPair,
    actionsByParentTool,
    parentToolByNamespace: deriveNamespaceAliases(records)
  };
}

let defaultIndex: ExecuteTargetIndex | undefined;

export function executeTargetIndex(): ExecuteTargetIndex {
  defaultIndex ??= buildExecuteTargetIndex(capabilityIndex().records);
  return defaultIndex;
}

function primaryLegacyPair(record: CapabilityRecord): LegacyPair {
  const first = record.legacyIds[0];
  return first === undefined
    ? { tool: record.routing.parentTool, action: record.routing.dispatchAction }
    : { tool: first.tool, action: first.action };
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
      message: 'execute requires either capability or tool + action. Call search before execute.',
      nextCall: buildNextCall({ operation: 'search' })
    });
  }

  const legacy = primaryLegacyPair(record);
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
