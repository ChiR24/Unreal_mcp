import { InMemoryTransport, Server } from '@modelcontextprotocol/server';
import { serveStdio } from '@modelcontextprotocol/server/stdio';
import { describe, expect, it } from 'vitest';

import { CACHE_HINTS, SERVER_CAPABILITIES, wirePrimitives } from './primitive-wiring.js';

const MODERN_META = {
  'io.modelcontextprotocol/protocolVersion': '2026-07-28',
  'io.modelcontextprotocol/clientCapabilities': {},
};

function buildServer(): Server {
  const server = new Server({ name: 'test-server', version: '9.9.9' }, {
    capabilities: SERVER_CAPABILITIES,
    instructions: 'HOW TO',
    cacheHints: CACHE_HINTS,
  });
  wirePrimitives(server);
  return server;
}

/** Sends one raw 2026-07-28 request, with no initialize, through the stdio serving entry and returns the reply. */
async function modernRequest(method: string): Promise<Record<string, unknown>> {
  const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
  const handle = serveStdio(buildServer, { transport: serverSide });
  const reply = new Promise<Record<string, unknown>>((resolve) => {
    clientSide.onmessage = (message) => resolve(message as Record<string, unknown>);
  });
  await clientSide.start();
  await clientSide.send({ jsonrpc: '2.0', id: 1, method, params: { _meta: MODERN_META } });
  const message = await reply;
  await handle.close();
  return message;
}

describe('server/discover (MCP 2026-07-28 discovery)', () => {
  it('answers without a session or handshake, with versions, capabilities, identity and cache hints', async () => {
    const reply = await modernRequest('server/discover');
    const result = reply.result as Record<string, unknown>;
    expect(result.resultType).toBe('complete');
    expect(result.supportedVersions, 'what a request names in its _meta; initialize negotiates the older ones').toEqual(['2026-07-28']);
    expect(result.capabilities).toEqual(SERVER_CAPABILITIES);
    expect(result.instructions).toBe('HOW TO');
    expect(result.ttlMs).toBe(3_600_000);
    expect(result.cacheScope).toBe('public');
    expect((result._meta as Record<string, unknown>)['io.modelcontextprotocol/serverInfo']).toEqual({ name: 'test-server', version: '9.9.9' });
  });

  it('is the same answer on every call', async () => {
    expect((await modernRequest('server/discover')).result).toEqual((await modernRequest('server/discover')).result);
  });
});

describe('2026-07-28 results carry the cache hints the native door gives', () => {
  it('prompts/list is a public, hour-long listing', async () => {
    const result = (await modernRequest('prompts/list')).result as Record<string, unknown>;
    expect(result.resultType).toBe('complete');
    expect(result.ttlMs).toBe(3_600_000);
    expect(result.cacheScope).toBe('public');
  });

  it('ping is gone in this revision', async () => {
    const reply = await modernRequest('ping');
    expect((reply.error as Record<string, unknown> | undefined)?.code).toBe(-32601);
  });
});
