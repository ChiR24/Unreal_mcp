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

/**
 * A WebSocket peer that answers every bridge_hello with `{ type: 'bridge_ack', ...ack }`
 * and hands any other frame to `onMessage`.
 */
export async function startAckServer(
    ack: Record<string, unknown> = {},
    onMessage?: (message: Record<string, unknown>, socket: WebSocket) => void
): Promise<{ server: WebSocketServer; port: number }> {
    const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
    server.on('connection', (socket) => {
        socket.on('message', (data) => {
            const message = JSON.parse(data.toString()) as Record<string, unknown>;
            if (message.type === 'bridge_hello') socket.send(JSON.stringify({ type: 'bridge_ack', ...ack }));
            else onMessage?.(message, socket);
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
