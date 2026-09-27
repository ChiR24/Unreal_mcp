// src/server/gateway/gateway-execute-resolve.ts
// Stage 1 of the canonical execute pipeline: the request-form types.
//
// This file owns the shapes every execute stage names (`LegacyPair`,
// `ExecuteTarget`, `ExecuteResolution`); the resolution
// algorithm itself lives in `gateway-execute-lookup.ts` and is re-exported here
// so callers keep a single import path for the whole resolve stage.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';

export type LegacyPair = { readonly tool: string; readonly action: string };

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

export { resolveExecuteTarget } from './gateway-execute-lookup.js';
