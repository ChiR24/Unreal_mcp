// src/server/gateway/gateway-execute-resolve.ts
// Stage 1 of the canonical execute pipeline: the request-form types and the
// index the resolve lookup runs over.
//
// This file owns the shapes every execute stage names (`LegacyPair`,
// `ExecuteTarget`, `ExecuteTargetIndex`, `ExecuteResolution`); the resolution
// algorithm itself lives in `gateway-execute-lookup.ts` and is re-exported here
// so callers keep a single import path for the whole resolve stage.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { capabilityIndex, deriveNamespaceAliases, legacyPairKey } from './gateway-capability-index.js';

export type LegacyPair = { readonly tool: string; readonly action: string };

export type ExecuteTargetIndex = {
  readonly ids: readonly string[];
  readonly parentTools: readonly string[];
  readonly byId: ReadonlyMap<string, CapabilityRecord>;
  /** Alias -> every capability that declares it, so a collision is visible. */
  readonly aliasOwners: ReadonlyMap<string, readonly string[]>;
  readonly byLegacyPair: ReadonlyMap<string, CapabilityRecord>;
  /** tool+action -> every capability that declares it, for the same reason. */
  readonly legacyPairOwners: ReadonlyMap<string, readonly string[]>;
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
  const legacyPairOwners = new Map<string, string[]>();
  const actionsByParentTool = new Map<string, string[]>();

  for (const record of records) {
    byId.set(record.id, record);
    for (const alias of record.aliases) {
      const owners = aliasOwners.get(alias) ?? [];
      owners.push(record.id);
      aliasOwners.set(alias, owners);
    }
    for (const legacy of record.legacyIds) {
      const pair = legacyPairKey(legacy.tool, legacy.action);
      byLegacyPair.set(pair, record);
      const pairOwners = legacyPairOwners.get(pair) ?? [];
      if (!pairOwners.includes(record.id)) pairOwners.push(record.id);
      legacyPairOwners.set(pair, pairOwners);
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
    legacyPairOwners,
    actionsByParentTool,
    parentToolByNamespace: deriveNamespaceAliases(records)
  };
}

let defaultIndex: ExecuteTargetIndex | undefined;

export function executeTargetIndex(): ExecuteTargetIndex {
  defaultIndex ??= buildExecuteTargetIndex(capabilityIndex().records);
  return defaultIndex;
}

export { resolveExecuteTarget } from './gateway-execute-lookup.js';
