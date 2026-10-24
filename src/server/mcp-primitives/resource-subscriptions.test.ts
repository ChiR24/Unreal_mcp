import { EventEmitter } from 'node:events';

import { InMemoryTransport, Server, type JSONRPCMessage } from '@modelcontextprotocol/server';
import { serveStdio } from '@modelcontextprotocol/server/stdio';
import { describe, expect, it } from 'vitest';

import { CACHE_HINTS, SERVER_CAPABILITIES, wirePrimitives } from './primitive-wiring.js';
import { CHANGES_URI, wireResourceSubscriptions } from './resource-subscriptions.js';

const MODERN_META = {
  'io.modelcontextprotocol/protocolVersion': '2026-07-28',
  'io.modelcontextprotocol/clientCapabilities': {},
};
const SUBSCRIPTION_ID = 'io.modelcontextprotocol/subscriptionId';

type Message = Record<string, unknown> & { id?: unknown; method?: string; params?: Record<string, unknown> };

/** A stdio connection through the SDK's own serving entry, with a change source the test moves. */
async function connect() {
  const changes = new EventEmitter();
  const [client, serverSide] = InMemoryTransport.createLinkedPair();
  const handle = serveStdio(() => {
    const server = new Server({ name: 'test-server', version: '9.9.9' }, { capabilities: SERVER_CAPABILITIES, cacheHints: CACHE_HINTS });
    wirePrimitives(server);
    wireResourceSubscriptions(server, changes);
    return server;
  }, { transport: serverSide });
  const received: Message[] = [];
  let wake: (() => void) | undefined;
  client.onmessage = (message) => {
    received.push(message as Message);
    wake?.();
  };
  await client.start();
  const next = async (match: (message: Message) => boolean): Promise<Message> => {
    for (;;) {
      const found = received.find(match);
      if (found) return found;
      await new Promise<void>((resolve) => { wake = resolve; });
    }
  };
  const send = (message: Record<string, unknown>): Promise<void> => client.send(message as JSONRPCMessage);
  /** Sends a request and waits for its answer: what the server sent before that answer has arrived. */
  const roundTrip = async (message: Record<string, unknown>): Promise<Message> => {
    await send(message);
    return next((reply) => reply.id === message.id && reply.method === undefined);
  };
  const updates = (): Message[] => received.filter((message) => message.method === 'notifications/resources/updated');
  return { changes, next, send, roundTrip, updates, close: () => handle.close() };
}

describe('ue://changes subscriptions (MCP 2026-07-28 subscriptions/listen)', () => {
  it('acknowledges the listen, then says when ue://changes moves, the listen id stamped on both', async () => {
    const connection = await connect();
    await connection.send({ jsonrpc: '2.0', id: 7, method: 'subscriptions/listen', params: { notifications: { resourceSubscriptions: [CHANGES_URI] }, _meta: MODERN_META } });
    const ack = await connection.next((message) => message.method === 'notifications/subscriptions/acknowledged');
    expect(ack.params).toEqual({ notifications: { resourceSubscriptions: [CHANGES_URI] }, _meta: { [SUBSCRIPTION_ID]: 7 } });

    connection.changes.emit('changesMoved', '3f9a1c2e.42');
    const updated = await connection.next((message) => message.method === 'notifications/resources/updated');
    expect(updated.params).toEqual({ uri: CHANGES_URI, _meta: { [SUBSCRIPTION_ID]: 7 } });
    await connection.close();
  });

  it('tells a listen that did not name ue://changes nothing', async () => {
    const connection = await connect();
    await connection.send({ jsonrpc: '2.0', id: 7, method: 'subscriptions/listen', params: { notifications: {}, _meta: MODERN_META } });
    await connection.next((message) => message.method === 'notifications/subscriptions/acknowledged');

    connection.changes.emit('changesMoved', '3f9a1c2e.42');
    await connection.roundTrip({ jsonrpc: '2.0', id: 8, method: 'server/discover', params: { _meta: MODERN_META } });
    expect(connection.updates()).toEqual([]);
    await connection.close();
  });
});

describe('ue://changes subscriptions (2025-era resources/subscribe)', () => {
  it('tells a client only what it subscribed to, until it unsubscribes', async () => {
    const connection = await connect();
    await connection.roundTrip({ jsonrpc: '2.0', id: 1, method: 'initialize', params: { protocolVersion: '2025-11-25', capabilities: {}, clientInfo: { name: 'test-client', version: '1.0.0' } } });
    await connection.send({ jsonrpc: '2.0', method: 'notifications/initialized' });

    connection.changes.emit('changesMoved', '3f9a1c2e.1');
    await connection.roundTrip({ jsonrpc: '2.0', id: 2, method: 'ping' });
    expect(connection.updates(), 'not subscribed yet').toEqual([]);

    expect((await connection.roundTrip({ jsonrpc: '2.0', id: 3, method: 'resources/subscribe', params: { uri: CHANGES_URI } })).result).toEqual({});
    connection.changes.emit('changesMoved', '3f9a1c2e.2');
    const updated = await connection.next((message) => message.method === 'notifications/resources/updated');
    expect(updated.params).toEqual({ uri: CHANGES_URI });

    await connection.roundTrip({ jsonrpc: '2.0', id: 4, method: 'resources/unsubscribe', params: { uri: CHANGES_URI } });
    connection.changes.emit('changesMoved', '3f9a1c2e.3');
    await connection.roundTrip({ jsonrpc: '2.0', id: 5, method: 'ping' });
    expect(connection.updates()).toHaveLength(1);
    await connection.close();
  });
});
