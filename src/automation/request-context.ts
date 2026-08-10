import { AsyncLocalStorage } from 'node:async_hooks';

/**
 * Carries the active MCP request identity through the TS stdio call stack so
 * that tool handlers (and the automation bridge they drive) can correlate
 * outbound Unreal automation work to the JSON-RPC request that spawned it —
 * without threading the id through every handler signature.
 */
export interface McpRequestContext {
    /** Canonicalized JSON-RPC request id (collision-free across string/number ids). */
    readonly requestId: string;
    /** The MCP SDK AbortSignal, fired when the client cancels the request. */
    readonly signal?: AbortSignal;
}

const mcpRequestContextStorage = new AsyncLocalStorage<McpRequestContext>();

/**
 * Run `fn` with `context` active on the async-local store. Any
 * `executeAutomationRequest` executed (transitively) inside `fn` can read it
 * via `getMcpRequestContext()` and tag its outbound automation request.
 */
export function runWithMcpRequestContext<T>(context: McpRequestContext, fn: () => T): T {
    return mcpRequestContextStorage.run(context, fn);
}

/** Read the currently active MCP request context, if any. */
export function getMcpRequestContext(): McpRequestContext | undefined {
    return mcpRequestContextStorage.getStore();
}

/**
 * Canonicalize a JSON-RPC request id into a collision-free string key.
 *
 * MCP request ids may be strings or numbers. `"1"` and `1` are distinct ids but
 * would otherwise map to the same string key, letting one request's
 * cancellation wrongly affect another. We namespace by type so the canonical
 * keys `num:1` and `str:1` never collide (and a string literally equal to
 * `num:1` stays distinct as `str:num:1`).
 */
export function canonicalizeMcpRequestId(id: string | number): string {
    return typeof id === 'number' ? `num:${id}` : `str:${id}`;
}
