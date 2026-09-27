// The server-under-test harness shared by the wire-level suites: a real
// createServer() under MOCK_UNREAL_CONNECTION, connected through an in-memory
// transport pair, either to an SDK Client or to a raw frame recorder.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { vi } from 'vitest';

import { isRecord } from '../../../src/utils/validation/type-guards.js';

type Built = ReturnType<typeof import('../../../src/server/server-factory.js').createServer>;
export type Frame = Record<string, unknown>;

// `resetModules` gives the server a fresh module generation. The frame harness
// skips it: its callers read request context from modules they imported at load,
// which must be the same instances the server uses.
async function freshServer(resetModules: boolean): Promise<Built> {
  if (resetModules) vi.resetModules();
  vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
  vi.stubEnv('NODE_ENV', 'test');
  const { createServer } = await import('../../../src/server/server-factory.js');
  return createServer();
}

async function closeServer(built: Built, clientSide: { close(): Promise<void> }): Promise<void> {
  await clientSide.close().catch(() => undefined);
  built.automationBridge.stop();
  vi.unstubAllEnvs();
}

/** A connected SDK client. Modules imported after this call share the server's module generation. */
export async function connectClient(name: string, bridge?: (tool: string, args?: Record<string, unknown>) => Promise<unknown>) {
  const built = await freshServer(true);
  if (bridge) {
    vi.spyOn(built.automationBridge, 'isConnected').mockReturnValue(true);
    vi.spyOn(built.automationBridge, 'sendAutomationRequest').mockImplementation(bridge);
  }
  const client = new Client({ name, version: '1.0.0' }, { capabilities: {} });
  const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
  await built.server.connect(serverSide);
  await client.connect(clientSide, { timeout: 15000 });
  return { built, client, close: () => closeServer(built, clientSide) };
}

/** A raw JSON-RPC frame recorder whose bridge answers every request with `bridge`. */
export async function connectFrames(bridge: () => Promise<unknown>) {
  const built = await freshServer(false);
  vi.spyOn(built.automationBridge, 'isConnected').mockReturnValue(true);
  vi.spyOn(built.automationBridge, 'sendAutomationRequest').mockImplementation(bridge);
  const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
  const frames: Frame[] = [];
  clientSide.onmessage = (message: unknown) => {
    frames.push(message as Frame);
  };
  await built.server.connect(serverSide);
  await clientSide.start();
  const send = (message: Frame) => clientSide.send(message as never);
  return { built, frames, send, close: () => closeServer(built, clientSide) };
}

/** Resolve as soon as `produce` yields a value, driven by the event loop rather than a fixed sleep. */
export async function waitFor<T>(produce: () => T | undefined): Promise<T> {
  for (let attempt = 0; attempt < 2000; attempt += 1) {
    const value = produce();
    if (value !== undefined) return value;
    await Promise.resolve();
    await new Promise<void>((resolve) => setImmediate(resolve));
  }
  throw new Error('condition never became true');
}

export async function initialize(ctx: { frames: readonly Frame[]; send: (message: Frame) => Promise<void> }, clientName: string): Promise<void> {
  await ctx.send({
    jsonrpc: '2.0',
    id: 1,
    method: 'initialize',
    params: { protocolVersion: '2025-11-25', capabilities: {}, clientInfo: { name: clientName, version: '1.0.0' } },
  });
  await waitFor(() => ctx.frames.find((frame) => frame.id === 1 && 'result' in frame));
  await ctx.send({ jsonrpc: '2.0', method: 'notifications/initialized' });
}

export const progressFrames = (frames: readonly Frame[]): Frame[] =>
  frames.filter((frame) => frame.method === 'notifications/progress');

/** A tool result's structuredContent, else its first JSON text part. */
export function structuredPayload(response: unknown): Record<string, unknown> {
  if (!isRecord(response)) return {};
  if (isRecord(response.structuredContent)) return response.structuredContent;
  const content = response.content;
  if (Array.isArray(content)) {
    for (const part of content) {
      if (isRecord(part) && typeof part.text === 'string') return JSON.parse(part.text) as Record<string, unknown>;
    }
  }
  return {};
}
