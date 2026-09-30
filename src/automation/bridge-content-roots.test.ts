// The editor's mount table reaches the path allowlist over the bridge: the
// initial list in bridge_ack, updates as content_roots_changed events, and
// nothing once the socket closes.
import { afterEach, describe, expect, it } from 'vitest';
import type { WebSocket, WebSocketServer } from 'ws';
import { clearEditorContentRoots, getEditorContentRoots } from '../utils/paths/path-security.js';
import { AutomationBridge } from './bridge.js';
import { closeServer, startAckServer } from './socket.test-support.js';
import type { AutomationBridgeAutomationEvent } from './types.js';

function sendToClients(server: WebSocketServer, message: Record<string, unknown>): void {
  for (const client of server.clients) client.send(JSON.stringify(message));
}

function connectedBridge(port: number): AutomationBridge {
  const bridge = new AutomationBridge({ host: '127.0.0.1', port, connectionTimeoutMs: 1000, heartbeatIntervalMs: 0 });
  bridge.on('error', () => undefined);
  return bridge;
}

function nextMessage(bridge: AutomationBridge): Promise<void> {
  return new Promise<void>(resolve => {
    bridge.once('message', () => resolve());
  });
}

describe('AutomationBridge editor content roots', () => {
  afterEach(() => {
    clearEditorContentRoots();
  });

  it('applies bridge_ack.contentRoots, follows content_roots_changed and clears them on close', async () => {
    const { server, port } = await startAckServer({ contentRoots: ['/Game', '/ShooterCore'] });
    const bridge = new AutomationBridge({
      host: '127.0.0.1',
      port,
      connectionTimeoutMs: 1000,
      heartbeatIntervalMs: 0
    });
    bridge.on('error', () => undefined);
    const events: AutomationBridgeAutomationEvent[] = [];
    bridge.on('automationEvent', event => {
      events.push(event);
    });

    try {
      expect(await bridge.connect()).toBe(true);
      expect(getEditorContentRoots()).toEqual(['/Game', '/ShooterCore']);

      let received = nextMessage(bridge);
      sendToClients(server, {
        type: 'automation_event',
        event: 'content_roots_changed',
        payload: { contentRoots: ['/Game', '/ShooterCore', '/NewFeature'] }
      });
      await received;
      expect(getEditorContentRoots()).toEqual(['/Game', '/ShooterCore', '/NewFeature']);
      // An internal bridge message: not forwarded to MCP clients.
      expect(events).toEqual([]);

      received = nextMessage(bridge);
      sendToClients(server, { type: 'automation_event', event: 'log', payload: { line: 'hello' } });
      await received;
      expect(events.map(event => event.event)).toEqual(['log']);

      const disconnected = new Promise<void>(resolve => {
        bridge.once('disconnected', () => resolve());
      });
      for (const client of server.clients) client.close(1000, 'bye');
      await disconnected;
      expect(getEditorContentRoots()).toEqual([]);
    } finally {
      bridge.stop();
      await closeServer(server);
    }
  });

  it('applies a content_roots_changed that arrives in the same read as bridge_ack', async () => {
    // ws emits every frame of one read in the same tick, so the message handler has to be
    // on the socket when bridge_ack is dispatched; otherwise the second frame is dropped.
    const { server, port } = await startAckServer({ contentRoots: ['/Game'] }, undefined, [{
      type: 'automation_event',
      event: 'content_roots_changed',
      payload: { contentRoots: ['/Game', '/ShooterCore'] }
    }]);
    const bridge = connectedBridge(port);
    const received = nextMessage(bridge);
    try {
      expect(await bridge.connect()).toBe(true);
      await Promise.race([received, new Promise(resolve => setTimeout(resolve, 1000))]);
      expect(getEditorContentRoots()).toEqual(['/Game', '/ShooterCore']);
    } finally {
      bridge.stop();
      await closeServer(server);
    }
  });

  it('clears them when a socket error unregisters the socket before it closes', async () => {
    const { server, port } = await startAckServer({ contentRoots: ['/Game', '/ShooterCore'] });
    const bridge = connectedBridge(port);
    try {
      expect(await bridge.connect()).toBe(true);
      const socket = (bridge as unknown as { connectionManager: { getSocket(): WebSocket } }).connectionManager.getSocket();
      const closed = new Promise<void>(resolve => {
        socket.once('close', () => resolve());
      });
      socket.emit('error', new Error('read ECONNRESET'));
      for (const client of server.clients) client.close(1000, 'bye');
      await closed;
      expect(getEditorContentRoots()).toEqual([]);
    } finally {
      bridge.stop();
      await closeServer(server);
    }
  });

  it('clears them when the bridge stops', async () => {
    const { server, port } = await startAckServer({ contentRoots: ['/Game', '/ShooterCore'] });
    const bridge = connectedBridge(port);
    try {
      expect(await bridge.connect()).toBe(true);
      expect(getEditorContentRoots()).toEqual(['/Game', '/ShooterCore']);
      bridge.stop();
      expect(getEditorContentRoots()).toEqual([]);
    } finally {
      bridge.stop();
      await closeServer(server);
    }
  });
});
