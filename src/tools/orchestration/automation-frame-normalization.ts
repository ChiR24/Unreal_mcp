// src/tools/orchestration/automation-frame-normalization.ts
//
// TASK 29 / T29-B6. One canonical structured payload for both transports.
//
// The native `/mcp` surface publishes the Unreal handler's `Result` object
// verbatim as `structuredContent`. The WebSocket bridge instead hands the whole
// automation frame back to TypeScript — `{type, requestId, success, message,
// error, result:{...domain payload...}}` — so the TS surface used to publish the
// domain payload one level down under `result`, beside transport-only keys no
// canonical record models. Every record's output schema is closed
// (`additionalProperties:false`), so that shape made correct Unreal output fail
// its own declared contract on TS for all 1,335 records.
//
// This module is the single normalization seam. It runs at the canonical 23-tool
// boundary (`handleConsolidatedToolCall`), so the gateway and the legacy
// direct-listing path are normalized by construction rather than by two adapters
// that could drift.

import { flattenPayloadWrappers } from '../../utils/responses/response-content.js';
import { isRecord } from '../../utils/validation/type-guards.js';

/** `type` values that mark a bridge frame rather than a domain payload. */
const TRANSPORT_FRAME_TYPES: ReadonlySet<string> = new Set([
  'automation_response',
  'automation_event'
]);

/** Frame-owned keys. Each is also a plausible domain field, so one is dropped ONLY if the domain payload does not claim it. */
const TRANSPORT_ONLY_KEYS = ['type', 'requestId', 'error'] as const;

const MAX_WRAPPER_DEPTH = 5;

function isTransportFrame(value: Record<string, unknown>): boolean {
  return typeof value.type === 'string' && TRANSPORT_FRAME_TYPES.has(value.type);
}

/** Some handlers return the frame verbatim, others re-wrap it under `data`; both leak transport keys. */
function findTransportFrame(value: Record<string, unknown>, depth = 0): Record<string, unknown> | undefined {
  if (isTransportFrame(value)) return value;
  if (depth >= MAX_WRAPPER_DEPTH) return undefined;
  if (isRecord(value.data)) {
    const viaData = findTransportFrame(value.data, depth + 1);
    if (viaData !== undefined) return viaData;
  }
  return isRecord(value.result) ? findTransportFrame(value.result, depth + 1) : undefined;
}

function reportsFailure(value: Record<string, unknown>): boolean {
  return value.success === false || value.isError === true;
}

/** Keys contributed by the frame's own payload, which are therefore domain fields, not transport ones. */
function domainOwnedKeys(frame: Record<string, unknown>): ReadonlySet<string> {
  const chain: Record<string, unknown> = {};
  if (isRecord(frame.data)) Object.assign(chain, frame.data);
  if (isRecord(frame.result)) Object.assign(chain, frame.result);
  return new Set(Object.keys(flattenPayloadWrappers(chain)));
}

/**
 * Project a handler result onto the payload a client actually receives.
 *
 * Failures are returned untouched: an error frame carries typed guidance and
 * diagnostic metadata that the gateway's `UNREAL_EXECUTION_ERROR` envelope and
 * the legacy `isError` promotion both read, and flattening one would risk
 * dressing a failure up as a success.
 */
export function normalizeAutomationFrame(result: unknown): unknown {
  if (!isRecord(result)) return result;

  const frame = findTransportFrame(result);
  if (frame === undefined) return result;
  // The frame carries the real outcome: a handler may wrap a FAILED frame in a
  // success-looking envelope, so both levels must be clear before flattening.
  if (reportsFailure(result) || reportsFailure(frame)) return result;

  const normalized = flattenPayloadWrappers(result);
  const domainKeys = domainOwnedKeys(frame);
  for (const key of TRANSPORT_ONLY_KEYS) {
    if (!domainKeys.has(key)) delete normalized[key];
  }
  return normalized;
}
