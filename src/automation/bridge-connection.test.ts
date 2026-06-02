import net from 'node:net';
import { WebSocketServer } from 'ws';
import { afterEach, describe, expect, it } from 'vitest';
import { AutomationBridge } from './bridge.js';

async function closeTcpServer(server: net.Server): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    server.close(error => {
      if (error) reject(error);
      else resolve();
    });
  });
}

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

describe('AutomationBridge lazy connection recovery', () => {
  const sockets: net.Socket[] = [];

  afterEach(() => {
    for (const socket of sockets.splice(0)) {
      socket.destroy();
    }
  });

  it('starts a fresh connection attempt after a lazy connection timeout', async () => {
    let connectionCount = 0;
    const server = net.createServer(socket => {
      connectionCount++;
      sockets.push(socket);
    });

    await new Promise<void>((resolve, reject) => {
      server.once('error', reject);
      server.listen(0, '127.0.0.1', resolve);
    });

    const address = server.address();
    if (!address || typeof address === 'string') {
      await closeTcpServer(server);
      throw new Error('Failed to bind test TCP server');
    }

    const bridge = new AutomationBridge({
      clientHost: '127.0.0.1',
      clientPort: address.port,
      connectionTimeoutMs: 50,
      heartbeatIntervalMs: 0
    });
    bridge.on('error', () => undefined);

    try {
      await expect(bridge.sendAutomationRequest('list', {}, { timeoutMs: 50 }))
        .rejects.toThrow(/Lazy connection timeout/);
      const firstConnectionCount = connectionCount;
      expect(firstConnectionCount).toBeGreaterThan(0);

      await expect(bridge.sendAutomationRequest('list', {}, { timeoutMs: 50 }))
        .rejects.toThrow(/Lazy connection timeout/);
      expect(connectionCount).toBeGreaterThan(firstConnectionCount);
    } finally {
      bridge.stop();
      for (const socket of sockets.splice(0)) {
        socket.destroy();
      }
      await closeTcpServer(server);
    }
  });

  it('rejects an in-flight lazy connection attempt immediately when stopped', async () => {
    const server = net.createServer(socket => {
      sockets.push(socket);
    });

    await new Promise<void>((resolve, reject) => {
      server.once('error', reject);
      server.listen(0, '127.0.0.1', resolve);
    });

    const address = server.address();
    if (!address || typeof address === 'string') {
      await closeTcpServer(server);
      throw new Error('Failed to bind test TCP server');
    }

    const bridge = new AutomationBridge({
      clientHost: '127.0.0.1',
      clientPort: address.port,
      connectionTimeoutMs: 5000,
      heartbeatIntervalMs: 0
    });
    bridge.on('error', () => undefined);

    try {
      const request = bridge.sendAutomationRequest('list', {}, { timeoutMs: 5000 });
      await new Promise(resolve => setTimeout(resolve, 25));

      bridge.stop();

      await expect(Promise.race([
        request,
        new Promise((_, reject) => setTimeout(() => reject(new Error('stop did not reject lazy attempt promptly')), 500))
      ])).rejects.toThrow(/server stopped/);
    } finally {
      bridge.stop();
      for (const socket of sockets.splice(0)) {
        socket.destroy();
      }
      await closeTcpServer(server);
    }
  });

  it('rejects queued requests when stopped before capacity is available', async () => {
    let holdRequestId = '';
    let releaseHold: (() => void) | undefined;
    const holdReceived = new Promise<void>(resolve => {
      releaseHold = resolve;
    });
    const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });

    server.on('connection', socket => {
      socket.on('message', data => {
        const text = typeof data === 'string' ? data : data.toString('utf8');
        const message = JSON.parse(text) as { type?: string; requestId?: string; action?: string };

        if (message.type === 'bridge_hello') {
          socket.send(JSON.stringify({ type: 'bridge_ack' }));
          return;
        }

        if (message.type === 'automation_request' && message.action === 'hold' && message.requestId) {
          holdRequestId = message.requestId;
          releaseHold?.();
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

    const bridge = new AutomationBridge({
      clientHost: '127.0.0.1',
      clientPort: address.port,
      connectionTimeoutMs: 1000,
      heartbeatIntervalMs: 0,
      maxPendingRequests: 1,
      maxQueuedRequests: 1
    });
    bridge.on('error', () => undefined);

    try {
      const inFlight = bridge.sendAutomationRequest('hold', {}, { timeoutMs: 5000 });
      await holdReceived;
      expect(holdRequestId).not.toBe('');

      const queued = bridge.sendAutomationRequest('queued', {}, { timeoutMs: 5000 });

      bridge.stop();

      await expect(inFlight).rejects.toThrow(/server stopped/);
      await expect(queued).rejects.toThrow(/server stopped/);
    } finally {
      bridge.stop();
      await closeWebSocketServer(server);
    }
  });
});
