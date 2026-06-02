import { EventEmitter } from 'node:events';
import { describe, expect, it, vi } from 'vitest';
import { WebSocket } from 'ws';
import { ConnectionManager } from './connection-manager.js';

class MockSocket extends EventEmitter {
  protocol = '';
  readyState = WebSocket.OPEN;
  ping = vi.fn();
  send = vi.fn();
  close = vi.fn();
}

function createSocket(): WebSocket {
  return new MockSocket() as unknown as WebSocket;
}

describe('ConnectionManager rate limiting', () => {
  it('resets message counts when the rate-limit window rolls over', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-01-01T00:00:00.000Z'));
    try {
      const manager = new ConnectionManager(0, 2, 0);
      const socket = createSocket();
      manager.registerSocket(socket, 8091);

      expect(manager.recordInboundMessage(socket, false)).toBe(true);
      expect(manager.recordInboundMessage(socket, false)).toBe(true);
      expect(manager.recordInboundMessage(socket, false)).toBe(false);

      vi.advanceTimersByTime(60_000);

      expect(manager.recordInboundMessage(socket, false)).toBe(true);
    } finally {
      vi.useRealTimers();
    }
  });

  it('rejects rate-limit accounting for sockets after removal', () => {
    const manager = new ConnectionManager(0, 2, 0);
    const socket = createSocket();
    manager.registerSocket(socket, 8091);
    manager.removeSocket(socket);

    expect(manager.recordInboundMessage(socket, false)).toBe(false);
  });

  it('rejects removed sockets even when rate limits are disabled', () => {
    const manager = new ConnectionManager(0, 0, 0);
    const socket = createSocket();
    manager.registerSocket(socket, 8091);
    manager.removeSocket(socket);

    expect(manager.recordInboundMessage(socket, false)).toBe(false);
  });

  it('allows exactly the configured message limit', () => {
    const manager = new ConnectionManager(0, 2, 0);
    const socket = createSocket();
    manager.registerSocket(socket, 8091);

    expect(manager.recordInboundMessage(socket, false)).toBe(true);
    expect(manager.recordInboundMessage(socket, false)).toBe(true);
    expect(manager.recordInboundMessage(socket, false)).toBe(false);
  });

  it('allows exactly the configured automation request limit', () => {
    const manager = new ConnectionManager(0, 0, 2);
    const socket = createSocket();
    manager.registerSocket(socket, 8091);

    expect(manager.recordInboundMessage(socket, true)).toBe(true);
    expect(manager.recordInboundMessage(socket, true)).toBe(true);
    expect(manager.recordInboundMessage(socket, true)).toBe(false);
  });
});
