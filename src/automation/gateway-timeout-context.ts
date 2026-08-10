import { AsyncLocalStorage } from 'node:async_hooks';

// `options.timeoutMs` is a gateway control, so it never travels as a handler
// param — the execute pipeline refuses control keys found in `params`. It rides
// async-local storage for the same reason `expectedRevisions` and `consent` do:
// the gateway does not merge `options` into the dispatched args, and threading a
// timeout through every domain handler signature to reach one bridge call would
// put a transport concern in 38 domains.
const gatewayTimeoutStorage = new AsyncLocalStorage<number>();

export function runWithGatewayTimeout<T>(timeoutMs: number, fn: () => T): T {
  return gatewayTimeoutStorage.run(timeoutMs, fn);
}

export function getGatewayTimeoutMs(): number | undefined {
  return gatewayTimeoutStorage.getStore();
}
