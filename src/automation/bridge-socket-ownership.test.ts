import { WebSocketServer, type WebSocket } from 'ws';
import { afterEach, describe, expect, it } from 'vitest';
import { AutomationBridge } from './bridge.js';
import type { AutomationBridgeConnectedEvent } from './types.js';

/**
 * Todo 7 (BB-006): clear request ownership when the primary WebSocket
 * disappears. Two sockets are registered against one mock WebSocketServer
 * (the reconnect-before-close race state) and the FIRST registered socket is
 * primary. The server holds a read on the primary, terminates the primary,
 * and this suite asserts the owner-scoped settlement contract: the held read
 * rejects promptly exactly once, the surviving secondary stays usable, the
 * request is never replayed, and no `cancel_request` frame is emitted for the
 * disconnect class (explicit non-notify, per the Todo 6 handoff).
 */

type RecordedFrame = { client: WebSocket; message: Record<string, unknown> };

async function closeWebSocketServer(server: WebSocketServer): Promise<void> {
  for (const client of server.clients) {
    client.terminate();
  }
  await new Promise<void>((resolve, reject) => {
    server.close(error => {
      if (error) reject(error);
      else resolve();
    });
  });
}

async function waitFor(predicate: () => boolean, description: string, timeoutMs = 3000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!predicate()) {
    if (Date.now() >= deadline) {
      throw new Error(`Timed out waiting for ${description}`);
    }
    await new Promise(resolve => setTimeout(resolve, 10));
  }
}

/** Race a promise against a deadline so a missing settlement fails the test promptly. */
function withTimeout<T>(promise: Promise<T>, timeoutMs: number, message: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), timeoutMs);
    promise.then(
      value => { clearTimeout(timer); resolve(value); },
      error => { clearTimeout(timer); reject(error); }
    );
  });
}

async function startMockServer(): Promise<{ server: WebSocketServer; port: number; frames: RecordedFrame[]; clients: () => WebSocket[] }> {
  const frames: RecordedFrame[] = [];
  const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
  server.on('connection', socket => {
    socket.on('message', data => {
      const text = typeof data === 'string' ? data : data.toString('utf8');
      const message = JSON.parse(text) as Record<string, unknown>;
      frames.push({ client: socket, message });
      if (message.type === 'bridge_hello') {
        socket.send(JSON.stringify({ type: 'bridge_ack' }));
      }
    });
  });
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.once('listening', () => resolve());
  });
  const address = server.address();
  if (!address || typeof address === 'string') {
    await closeWebSocketServer(server);
    throw new Error('Failed to bind test WebSocket server');
  }
  return { server, port: address.port, frames, clients: () => Array.from(server.clients) };
}

/** Start the bridge twice against one server: FIRST registered socket is primary. */
async function startTwoSocketBridge(mock: { server: WebSocketServer; port: number }): Promise<AutomationBridge> {
  const bridge = new AutomationBridge({
    clientHost: '127.0.0.1',
    clientPort: mock.port,
    connectionTimeoutMs: 5000,
    heartbeatIntervalMs: 0
  });
  bridge.on('error', () => undefined);
  const connected: AutomationBridgeConnectedEvent[] = [];
  bridge.on('connected', info => connected.push(info));
  bridge.start();
  await waitFor(() => connected.length >= 1, 'first socket registration');
  bridge.start();
  await waitFor(() => connected.length >= 2, 'second socket registration');
  return bridge;
}

const automationFrames = (frames: RecordedFrame[]) => frames.filter(f => f.message.type === 'automation_request');
const cancelFrames = (frames: RecordedFrame[]) => frames.filter(f => f.message.type === 'cancel_request');
const payloadOf = (frame: RecordedFrame): Record<string, unknown> | undefined =>
  frame.message.payload && typeof frame.message.payload === 'object'
    ? frame.message.payload as Record<string, unknown>
    : undefined;

describe('AutomationBridge socket ownership (Todo 7 / BB-006)', () => {
  const bridges: AutomationBridge[] = [];

  afterEach(async () => {
    for (const bridge of bridges.splice(0)) {
      bridge.stop();
    }
  });

  it('rejects the pending read owned by a lost primary and keeps the surviving secondary usable', async () => {
    const mock = await startMockServer();
    const bridge = await startTwoSocketBridge(mock);
    bridges.push(bridge);
    try {
      const [primaryClient, secondaryClient] = mock.clients();
      expect(primaryClient).toBeDefined();
      expect(secondaryClient).toBeDefined();

      // Hold a read R1 on the primary; the server records the frame and never replies.
      const r1 = bridge.sendAutomationRequest('get_actor', { actor: 'Hero' }, { timeoutMs: 10_000 });
      void r1.catch(() => undefined); // keep rejection handled even if a later assertion fails first
      await waitFor(() => automationFrames(mock.frames).some(f => payloadOf(f)?.actor === 'Hero'), 'R1 frame on the primary');
      const r1Frame = automationFrames(mock.frames).find(f => payloadOf(f)?.actor === 'Hero');
      expect(r1Frame?.client).toBe(primaryClient);
      expect(bridge.getStatus().pendingRequests).toBe(1);

      // Terminate the primary: R1 must settle promptly (well before its 10s deadline).
      primaryClient.terminate();
      await expect(withTimeout(r1, 1500, 'R1 was not rejected promptly after primary loss'))
        .rejects.toThrow(/primary connection lost/);

      // Ownership settlement is complete and the secondary survives.
      expect(bridge.getStatus().pendingRequests).toBe(0);
      expect(bridge.isConnected()).toBe(true);
      expect(cancelFrames(mock.frames)).toHaveLength(0);

      // A new read R2 succeeds over the promoted secondary.
      const r2 = bridge.sendAutomationRequest('get_actor', { actor: 'Sidekick' }, { timeoutMs: 5000 });
      await waitFor(() => automationFrames(mock.frames).some(f => payloadOf(f)?.actor === 'Sidekick'), 'R2 frame on the secondary');
      const r2Frame = automationFrames(mock.frames).find(f => payloadOf(f)?.actor === 'Sidekick');
      expect(r2Frame?.client).toBe(secondaryClient);
      expect(r2Frame?.message.requestId).not.toBe(r1Frame?.message.requestId);

      secondaryClient.send(JSON.stringify({
        type: 'automation_response',
        requestId: r2Frame?.message.requestId,
        success: true,
        result: { actor: 'Sidekick' }
      }));
      await expect(withTimeout(r2, 1500, 'R2 did not resolve over the surviving secondary'))
        .resolves.toMatchObject({ success: true });

      // Exactly one original frame per request: no replay, no duplicate mutation.
      expect(automationFrames(mock.frames).filter(f => payloadOf(f)?.actor === 'Hero')).toHaveLength(1);
      expect(automationFrames(mock.frames).filter(f => payloadOf(f)?.actor === 'Sidekick')).toHaveLength(1);
      expect(cancelFrames(mock.frames)).toHaveLength(0);

      // A late response for the settled R1 is ignored (no entry remains).
      secondaryClient.send(JSON.stringify({
        type: 'automation_response',
        requestId: r1Frame?.message.requestId,
        success: true,
        result: { actor: 'Hero' }
      }));
      expect(bridge.getStatus().pendingRequests).toBe(0);
    } finally {
      bridge.stop();
      await closeWebSocketServer(mock.server);
    }
  });

  it('rejects every coalesced subscriber of a lost primary exactly once without a second frame', async () => {
    const mock = await startMockServer();
    const bridge = await startTwoSocketBridge(mock);
    bridges.push(bridge);
    try {
      const [primaryClient] = mock.clients();
      expect(primaryClient).toBeDefined();

      // Two identical reads coalesce onto ONE underlying automation request.
      const r1a = bridge.sendAutomationRequest('get_actor', { actor: 'Hero' }, { timeoutMs: 10_000 });
      const r1b = bridge.sendAutomationRequest('get_actor', { actor: 'Hero' }, { timeoutMs: 10_000 });
      void r1a.catch(() => undefined);
      void r1b.catch(() => undefined);
      await waitFor(() => automationFrames(mock.frames).some(f => payloadOf(f)?.actor === 'Hero'), 'coalesced R1 frame');
      await new Promise(resolve => setTimeout(resolve, 50));
      expect(automationFrames(mock.frames).filter(f => payloadOf(f)?.actor === 'Hero')).toHaveLength(1);

      primaryClient.terminate();

      await expect(withTimeout(r1a, 1500, 'coalesced subscriber A was not rejected promptly'))
        .rejects.toThrow(/primary connection lost/);
      await expect(withTimeout(r1b, 1500, 'coalesced subscriber B was not rejected promptly'))
        .rejects.toThrow(/primary connection lost/);
      expect(bridge.getStatus().pendingRequests).toBe(0);
      expect(automationFrames(mock.frames).filter(f => payloadOf(f)?.actor === 'Hero')).toHaveLength(1);
      expect(cancelFrames(mock.frames)).toHaveLength(0);
    } finally {
      bridge.stop();
      await closeWebSocketServer(mock.server);
    }
  });

  it('rejects owned pending and queued requests when the last socket closes (full teardown retained)', async () => {
    const mock = await startMockServer();
    const bridge = new AutomationBridge({
      clientHost: '127.0.0.1',
      clientPort: mock.port,
      connectionTimeoutMs: 5000,
      heartbeatIntervalMs: 0,
      maxPendingRequests: 1,
      maxQueuedRequests: 1
    });
    bridges.push(bridge);
    bridge.on('error', () => undefined);
    const connected: AutomationBridgeConnectedEvent[] = [];
    bridge.on('connected', info => connected.push(info));
    bridge.start();
    await waitFor(() => connected.length >= 1, 'single socket registration');
    try {
      const [onlyClient] = mock.clients();
      expect(onlyClient).toBeDefined();

      const held = bridge.sendAutomationRequest('hold', {}, { timeoutMs: 5000 });
      await waitFor(() => automationFrames(mock.frames).some(f => f.message.action === 'hold'), 'held frame');
      const queued = bridge.sendAutomationRequest('queued', {}, { timeoutMs: 5000 });
      void held.catch(() => undefined);
      void queued.catch(() => undefined);
      expect(automationFrames(mock.frames).filter(f => f.message.action === 'hold')).toHaveLength(1);

      onlyClient.terminate();

      // Held (owned) and queued (never sent) both settle via the retained
      // all-pending full-teardown path when the last socket closes.
      await expect(withTimeout(held, 1500, 'held request was not rejected on last-socket close'))
        .rejects.toThrow(/Connection lost/);
      await expect(withTimeout(queued, 1500, 'queued request was not rejected on full teardown'))
        .rejects.toThrow(/Connection lost/);
      expect(bridge.getStatus().pendingRequests).toBe(0);
      expect(cancelFrames(mock.frames)).toHaveLength(0);
    } finally {
      bridge.stop();
      await closeWebSocketServer(mock.server);
    }
  });
});
