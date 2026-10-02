import net from 'node:net';
import { afterEach, describe, expect, it } from 'vitest';
import { AutomationBridge } from './bridge.js';
import { closeServer, listenTcp, startAckServer } from './socket.test-support.js';

describe('AutomationBridge lazy connection recovery', () => {
  const sockets: net.Socket[] = [];

  afterEach(() => {
    for (const socket of sockets.splice(0)) {
      socket.destroy();
    }
  });

  it('reports the dialed client URL, not the listen host and ports', async () => {
    const bridge = new AutomationBridge({
      host: '::1',
      port: 8099,
      connectionTimeoutMs: 200,
      heartbeatIntervalMs: 0
    });
    bridge.on('error', () => undefined);

    try {
      const url = bridge.getClientUrl();
      expect(url).toBe('ws://[::1]:8099');
      await expect(bridge.sendAutomationRequest('list', {}, { timeoutMs: 200 }))
        .rejects.toThrow(`Automation bridge not connected at ${url}`);
    } finally {
      bridge.stop();
    }
  });

  it('starts a fresh connection attempt after a lazy connection timeout', async () => {
    let connectionCount = 0;
    const server = net.createServer(socket => {
      connectionCount++;
      sockets.push(socket);
    });

    const port = await listenTcp(server);

    // 50 ms was too short on a loaded machine: the attempt timed out before the server saw the connection.
    const bridge = new AutomationBridge({
      host: '127.0.0.1',
      port,
      connectionTimeoutMs: 250,
      heartbeatIntervalMs: 0
    });
    bridge.on('error', () => undefined);

    try {
      await expect(bridge.sendAutomationRequest('list', {}, { timeoutMs: 250 }))
        .rejects.toThrow(/timed out/);
      const firstConnectionCount = connectionCount;
      expect(firstConnectionCount).toBeGreaterThan(0);

      await expect(bridge.sendAutomationRequest('list', {}, { timeoutMs: 250 }))
        .rejects.toThrow(/timed out/);
      expect(connectionCount).toBeGreaterThan(firstConnectionCount);
    } finally {
      bridge.stop();
      for (const socket of sockets.splice(0)) {
        socket.destroy();
      }
      await closeServer(server);
    }
  });

  it('rejects an in-flight lazy connection attempt immediately when stopped', async () => {
    const server = net.createServer(socket => {
      sockets.push(socket);
    });

    const port = await listenTcp(server);

    const bridge = new AutomationBridge({
      host: '127.0.0.1',
      port,
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
      await closeServer(server);
    }
  });

  it('rejects queued requests when stopped before capacity is available', async () => {
    let holdRequestId = '';
    let releaseHold: (() => void) | undefined;
    const holdReceived = new Promise<void>(resolve => {
      releaseHold = resolve;
    });
    const { server, port } = await startAckServer({}, (message) => {
      if (message.type === 'automation_request' && message.action === 'hold' && typeof message.requestId === 'string') {
        holdRequestId = message.requestId;
        releaseHold?.();
      }
    });

    const bridge = new AutomationBridge({
      host: '127.0.0.1',
      port,
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
      await closeServer(server);
    }
  });
});
