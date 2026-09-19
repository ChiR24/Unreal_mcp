import { AsyncLocalStorage } from 'node:async_hooks';

import type { ConsentGrant } from '../tools/catalog/capabilities/semantic/authorization.js';
import type { ExpectedRevisions } from '../tools/catalog/capabilities/semantic/execution-options.js';
import type { CorrelationId } from '../tools/catalog/capabilities/semantic/ids.js';

/**
 * The gateway-scoped async-local values.
 *
 * These are gateway *controls*, not handler params: the execute pipeline
 * refuses control keys found in `params` and never merges `options` into the
 * dispatched args, so threading any of them through every domain handler
 * signature would put a gateway concern in 38 domains. They ride async-local
 * storage instead, set once in the gateway and read at the bridge boundary.
 *
 * This was four files repeating the same three-line storage/run/get shape; a
 * fifth control should be added here rather than copied into a fifth file.
 */
function createGatewayContext<T>(): {
  run: <R>(value: T, fn: () => R) => R;
  get: () => T | undefined;
} {
  const storage = new AsyncLocalStorage<T>();
  return {
    run: (value, fn) => storage.run(value, fn),
    get: () => storage.getStore()
  };
}

const consent = createGatewayContext<ConsentGrant>();
const expectedRevisions = createGatewayContext<ExpectedRevisions>();
const timeoutMs = createGatewayContext<number>();

/**
 * The single client-facing gateway correlation id (`gw-N`), minted once in
 * `handleUnrealGatewayCall` and echoed identically at every hop: the gateway log
 * line, the outbound automation request metadata, and the semantic receipt.
 * Distinct from `request-context.ts`, which carries the external MCP request id
 * and the SDK AbortSignal.
 */
const correlation = createGatewayContext<CorrelationId>();

export function runWithGatewayConsent<T>(grant: ConsentGrant, fn: () => T): T {
  return consent.run(grant, fn);
}

export function getGatewayConsent(): ConsentGrant | undefined {
  return consent.get();
}

export function runWithGatewayExpectedRevisions<T>(revisions: ExpectedRevisions, fn: () => T): T {
  return expectedRevisions.run(revisions, fn);
}

export function getGatewayExpectedRevisions(): ExpectedRevisions | undefined {
  return expectedRevisions.get();
}

export function runWithGatewayTimeout<T>(value: number, fn: () => T): T {
  return timeoutMs.run(value, fn);
}

export function getGatewayTimeoutMs(): number | undefined {
  return timeoutMs.get();
}

export function runWithGatewayCorrelation<T>(correlationId: CorrelationId, fn: () => T): T {
  return correlation.run(correlationId, fn);
}

export function getGatewayCorrelationId(): CorrelationId | undefined {
  return correlation.get();
}
