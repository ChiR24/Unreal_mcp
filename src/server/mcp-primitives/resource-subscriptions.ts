// src/server/mcp-primitives/resource-subscriptions.ts
// Subscriptions to ue://changes, the one resource that moves while the server runs (the editor's change journal; the
// plugin says it moved with a changes_moved event, at most one a second). A 2026-07-28 client asks with
// subscriptions/listen, which the SDK's stdio entry serves itself: it acknowledges the filter and hands each change
// notification only to the streams that named its URI. A 2025-era client sends resources/subscribe, answered here,
// and the SDK passes such a client everything it is given, so it hears only of what it subscribed to. Each Server
// instance binds its own listener and drops it when it closes. The native door does the same in
// Private/MCP/Protocol/McpNativeTransportListen.cpp.

import type { Server } from '@modelcontextprotocol/server';

export const CHANGES_URI = 'ue://changes';

/** The first revision with subscriptions/listen; an instance bound to an earlier one subscribes per resource. */
const LISTEN_REVISION = '2026-07-28';

export interface ChangeSource {
  on(event: 'changesMoved', listener: (cursor: string) => void): unknown;
  off(event: 'changesMoved', listener: (cursor: string) => void): unknown;
}

/** Answers resources/subscribe and unsubscribe, and tells the client when ue://changes moves; returns the unbind. */
export function wireResourceSubscriptions(server: Server, changes: ChangeSource): () => void {
  // Any resource may be subscribed to; only ue://changes ever moves.
  const subscribed = new Set<string>();
  server.setRequestHandler('resources/subscribe', (request) => {
    subscribed.add(request.params.uri);
    return {};
  });
  server.setRequestHandler('resources/unsubscribe', (request) => {
    subscribed.delete(request.params.uri);
    return {};
  });
  const onMoved = (): void => {
    // Version strings are dates, so plain string order is their order.
    const listens = (server.getNegotiatedProtocolVersion() ?? '') >= LISTEN_REVISION;
    if (!listens && !subscribed.has(CHANGES_URI)) return;
    server.sendResourceUpdated({ uri: CHANGES_URI }).catch(() => { /* a closed connection hears nothing more */ });
  };
  changes.on('changesMoved', onMoved);
  return () => {
    changes.off('changesMoved', onMoved);
  };
}
