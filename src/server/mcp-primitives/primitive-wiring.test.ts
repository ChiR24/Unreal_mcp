import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { SUPPORTED_PROTOCOL_VERSIONS } from '@modelcontextprotocol/sdk/types.js';
import { describe, expect, it } from 'vitest';

import { SERVER_CAPABILITIES, wirePrimitives } from './primitive-wiring.js';

/** Sends one raw JSON-RPC request with no initialize first, as a 2026-07-28 client may, and returns the reply. */
async function rawRequest(method: string): Promise<Record<string, unknown>> {
  const server = new Server({ name: 'test-server', version: '9.9.9' }, { capabilities: SERVER_CAPABILITIES, instructions: 'HOW TO' });
  wirePrimitives(server, { name: 'test-server', version: '9.9.9' }, 'HOW TO');
  const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
  await server.connect(serverSide);
  const reply = new Promise<Record<string, unknown>>((resolve) => {
    clientSide.onmessage = (message) => resolve(message as Record<string, unknown>);
  });
  await clientSide.start();
  await clientSide.send({ jsonrpc: '2.0', id: 1, method, params: {} });
  const message = await reply;
  await server.close();
  return message;
}

describe('server/discover (MCP 2026-07-28 discovery)', () => {
  it('answers without a session or handshake, with versions, capabilities, identity and cache hints', async () => {
    const reply = await rawRequest('server/discover');
    const result = reply.result as Record<string, unknown>;
    expect(result.resultType).toBe('complete');
    expect(result.supportedVersions, 'only the revisions initialize negotiates, so a newer client falls back to it').toEqual([...SUPPORTED_PROTOCOL_VERSIONS]);
    expect(result.capabilities).toEqual(SERVER_CAPABILITIES);
    expect(result.instructions).toBe('HOW TO');
    expect(result.ttlMs).toBeGreaterThan(0);
    expect(result.cacheScope).toBe('public');
    expect(result._meta).toEqual({ 'io.modelcontextprotocol/serverInfo': { name: 'test-server', version: '9.9.9' } });
  });

  it('is the same answer on every call', async () => {
    expect((await rawRequest('server/discover')).result).toEqual((await rawRequest('server/discover')).result);
  });
});
