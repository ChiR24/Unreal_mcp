// src/services/telemetry-observation.ts
// Task 47: derive the BOUNDED telemetry dimensions from a real gateway call.
//
// The action class comes from the canonical capability's `policy.requiredScope`,
// which is the same field the plugin's pre-queue gate reads when it resolves a
// demand. Both surfaces therefore answer "how dangerous was this" from one
// authority instead of re-deriving it from an action string.
//
// Nothing here ever returns a capability id, path, parameter value or error
// message. The return types are the closed schema unions, so a value that does
// not resolve degrades to `unknown` and can never become a metric label.

import { resolveCapability, resolveLegacyPair } from '../server/gateway/gateway-capability-index.js';
import {
  coerceActionClass,
  coerceFailureClass,
  type TelemetryActionClass,
  type TelemetryFailureClass,
} from './telemetry-schema.js';
import { isRecord } from '../utils/validation/type-guards.js';

function stringField(source: Record<string, unknown>, key: string): string | undefined {
  const value = source[key];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

/**
 * Classify one `unreal` gateway call. Discovery operations (search, describe,
 * configure) read the catalogue and are classified `read`; an execute is
 * classified by the resolved capability's required scope.
 */
export function actionClassForGatewayArgs(args: unknown): TelemetryActionClass {
  if (!isRecord(args)) return 'unknown';

  const operation = stringField(args, 'operation');
  if (operation !== undefined && operation !== 'execute') {
    return 'read';
  }

  const capability = stringField(args, 'capability');
  if (capability !== undefined) {
    const resolution = resolveCapability(capability);
    if (resolution.kind !== 'unknown') {
      return coerceActionClass(resolution.record.policy.requiredScope);
    }
    return 'unknown';
  }

  const tool = stringField(args, 'tool');
  const action = stringField(args, 'action');
  if (tool !== undefined && action !== undefined) {
    const resolution = resolveLegacyPair(tool, action);
    if (resolution.kind !== 'unknown') {
      return coerceActionClass(resolution.record.policy.requiredScope);
    }
  }

  return 'unknown';
}

const TRANSPORT_MARKERS = ['not connected', 'connection lost', 'failed to send', 'socket', 'econnrefused'];
const TIMEOUT_MARKERS = ['timed out', 'timeout'];

/**
 * Classify a failure. A machine-readable code is preferred and is bounded by
 * `coerceFailureClass`; only when no code resolves does this fall back to a
 * small set of substring markers. The message itself is NEVER returned - it
 * routinely carries asset paths and object names.
 */
export function failureClassForError(value: unknown): TelemetryFailureClass {
  if (isRecord(value)) {
    for (const key of ['errorCode', 'code', 'errorType']) {
      const candidate = stringField(value, key);
      if (candidate !== undefined) {
        const bounded = coerceFailureClass(candidate);
        if (bounded !== 'unknown') return bounded;
      }
    }
    const debug = value._debug;
    if (isRecord(debug)) {
      const candidate = stringField(debug, 'errorType');
      if (candidate !== undefined) {
        const bounded = coerceFailureClass(candidate);
        if (bounded !== 'unknown') return bounded;
      }
    }
  }

  const message = value instanceof Error ? value.message : isRecord(value) ? stringField(value, 'error') : undefined;
  if (message === undefined) return 'unknown';

  const lowered = message.toLowerCase();
  if (TIMEOUT_MARKERS.some((marker) => lowered.includes(marker))) return 'timeout';
  if (TRANSPORT_MARKERS.some((marker) => lowered.includes(marker))) return 'transport';
  return 'unknown';
}
