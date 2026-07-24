import { AsyncLocalStorage } from 'node:async_hooks';

import type { CorrelationId } from '../tools/catalog/capabilities/semantic/ids.js';

/**
 * Carries the single client-facing gateway correlation id through the TS execute
 * call stack so the automation bridge can tag its outbound request metadata with
 * the SAME id that lands on the receipt — without threading it through any
 * handler signature or contaminating action params.
 *
 * Distinct from `request-context.ts`, which carries the external MCP request id
 * and the SDK AbortSignal. This id is the gateway's own per-execute join key
 * (`gw-N`), minted once in `handleUnrealGatewayCall` and echoed identically at
 * every hop: the gateway log line, the outbound automation request metadata, and
 * the semantic receipt.
 */
const gatewayCorrelationStorage = new AsyncLocalStorage<CorrelationId>();

/**
 * Run `fn` with `correlationId` active on the async-local store. Any
 * `executeAutomationRequest` executed (transitively) inside `fn` reads it via
 * `getGatewayCorrelationId()` and stamps its outbound automation request
 * metadata with the same value.
 */
export function runWithGatewayCorrelation<T>(correlationId: CorrelationId, fn: () => T): T {
  return gatewayCorrelationStorage.run(correlationId, fn);
}

/** Read the currently active gateway correlation id, if any. */
export function getGatewayCorrelationId(): CorrelationId | undefined {
  return gatewayCorrelationStorage.getStore();
}
