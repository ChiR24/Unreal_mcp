// Shared servers for the bridge connection tests: a raw TCP listener and a
// WebSocket peer that answers bridge_hello, each on an ephemeral loopback port.
import net from 'node:net';
import { WebSocket, WebSocketServer } from 'ws';

function portOf(address: string | net.AddressInfo | null): number {
    if (!address || typeof address === 'string') throw new Error('Failed to bind test server');
    return address.port;
}

/** Listen on an ephemeral loopback port and return it. */
export async function listenTcp(server: net.Server): Promise<number> {
    await new Promise<void>((resolve, reject) => {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', resolve);
    });
    return portOf(server.address());
}

/** One unmasked server-to-client text frame (RFC 6455 section 5.2); the payload stays under 64 KiB. */
function textFrame(message: Record<string, unknown>): Buffer {
    const payload = Buffer.from(JSON.stringify(message), 'utf8');
    if (payload.length > 0xffff) throw new Error('textFrame: payload over 64 KiB');
    const header = payload.length < 126
        ? Buffer.from([0x81, payload.length])
        : Buffer.from([0x81, 126, payload.length >> 8, payload.length & 0xff]);
    return Buffer.concat([header, payload]);
}

/**
 * A WebSocket peer that answers every bridge_hello with `{ type: 'bridge_ack', ...ack }`
 * and hands any other frame to `onMessage`. The `sameWrite` frames follow the ack in
 * one TCP write, so the client reads them together with it.
 */
export async function startAckServer(
    ack: Record<string, unknown> = {},
    onMessage?: (message: Record<string, unknown>, socket: WebSocket) => void,
    sameWrite: readonly Record<string, unknown>[] = []
): Promise<{ server: WebSocketServer; port: number }> {
    const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
    server.on('connection', (socket) => {
        socket.on('message', (data) => {
            const message = JSON.parse(data.toString()) as Record<string, unknown>;
            if (message.type !== 'bridge_hello') {
                onMessage?.(message, socket);
                return;
            }
            const ackMessage = { type: 'bridge_ack', ...ack };
            if (sameWrite.length === 0) {
                socket.send(JSON.stringify(ackMessage));
                return;
            }
            // send() writes one frame per call; the raw socket takes all of them in one write.
            const raw = (socket as unknown as { _socket: net.Socket })._socket;
            raw.write(Buffer.concat([ackMessage, ...sameWrite].map(textFrame)));
        });
    });
    await new Promise<void>((resolve, reject) => {
        server.once('error', reject);
        server.once('listening', () => resolve());
    });
    return { server, port: portOf(server.address()) };
}

/** Close a TCP or WebSocket server; WebSocket clients are terminated first. */
export async function closeServer(server: net.Server | WebSocketServer): Promise<void> {
    if (server instanceof WebSocketServer) for (const client of server.clients) client.terminate();
    await new Promise<void>((resolve, reject) => {
        server.close((error?: Error) => (error ? reject(error) : resolve()));
    });
}
