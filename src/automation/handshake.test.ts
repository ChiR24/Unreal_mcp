import { EventEmitter } from 'node:events';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { WebSocket } from 'ws';
import { Logger } from '../utils/logging/logger.js';
import { HandshakeHandler } from './handshake.js';

class FakeSocket extends EventEmitter {
    protocol = 'ws';
    readyState = WebSocket.OPEN;
    sent: string[] = [];
    close = vi.fn();
    ping = vi.fn();

    send(payload: string): void {
        this.sent.push(payload);
    }
}

describe('HandshakeHandler', () => {
    let debugMessages: string[];

    beforeEach(() => {
        vi.useFakeTimers();
        debugMessages = [];
        vi.spyOn(Logger.prototype, 'debug').mockImplementation((...args: unknown[]) => {
            debugMessages.push(args.map(arg => String(arg)).join(' '));
        });
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.restoreAllMocks();
    });

    it('sends but does not log capability tokens', async () => {
        const socket = new FakeSocket();
        const promise = new HandshakeHandler('secret-token').initiateHandshake(socket, 1000);

        await vi.advanceTimersByTimeAsync(0);

        expect(socket.sent).toHaveLength(1);
        expect(socket.sent[0]).toContain('secret-token');

        const debugOutput = debugMessages.join('\n');
        expect(debugOutput).not.toContain('secret-token');

        socket.emit('message', JSON.stringify({ type: 'bridge_ack' }));

        await expect(promise).resolves.toEqual({});
    });


    it('settles timeout failures once and removes listeners', async () => {
        const socket = new FakeSocket();
        const promise = new HandshakeHandler().initiateHandshake(socket, 1000);
        const assertion = expect(promise).rejects.toThrow('Handshake timeout');

        await vi.advanceTimersByTimeAsync(1000);

        await assertion;
        expect(socket.close).toHaveBeenCalledOnce();
        expect(socket.listenerCount('message')).toBe(0);
        expect(socket.listenerCount('error')).toBe(0);
        expect(socket.listenerCount('close')).toBe(0);
    });

    it('keeps the contentRoots of bridge_ack in the handshake metadata', async () => {
        const socket = new FakeSocket();
        const promise = new HandshakeHandler().initiateHandshake(socket, 1000);

        await vi.advanceTimersByTimeAsync(0);
        socket.emit('message', JSON.stringify({ type: 'bridge_ack', contentRoots: ['/Game', '/ShooterCore'] }));

        await expect(promise).resolves.toEqual({ contentRoots: ['/Game', '/ShooterCore'] });
    });

    it('runs onAcknowledged inside the bridge_ack dispatch, before the next frame', async () => {
        const socket = new FakeSocket();
        const after: string[] = [];
        const promise = new HandshakeHandler().initiateHandshake(socket, 1000, () => {
            socket.on('message', (data: string) => after.push(data));
        });

        await vi.advanceTimersByTimeAsync(0);
        const next = JSON.stringify({ type: 'automation_event', event: 'content_roots_changed', payload: { contentRoots: ['/Game'] } });
        // Two frames of one read: ws emits them back to back, with no await in between.
        socket.emit('message', JSON.stringify({ type: 'bridge_ack' }));
        socket.emit('message', next);

        expect(after).toEqual([next]);
        await expect(promise).resolves.toEqual({});
    });

    it('rejects the handshake when onAcknowledged throws', async () => {
        const socket = new FakeSocket();
        const promise = new HandshakeHandler().initiateHandshake(socket, 1000, () => {
            throw new Error('register failed');
        });
        const assertion = expect(promise).rejects.toThrow('register failed');

        await vi.advanceTimersByTimeAsync(0);
        socket.emit('message', JSON.stringify({ type: 'bridge_ack' }));

        await assertion;
        expect(socket.listenerCount('message')).toBe(0);
    });

    it('refuses a bridge_ack whose contentRoots is not a list of strings', async () => {
        const socket = new FakeSocket();
        const promise = new HandshakeHandler().initiateHandshake(socket, 1000);
        const assertion = expect(promise).rejects.toThrow('Handshake expected bridge_ack');

        await vi.advanceTimersByTimeAsync(0);
        socket.emit('message', JSON.stringify({ type: 'bridge_ack', contentRoots: '/Game' }));

        await assertion;
    });
});
