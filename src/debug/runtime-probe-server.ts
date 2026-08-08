import { randomBytes } from 'node:crypto';
import net from 'node:net';
import type { DebugCorrelationContext } from './types.js';

const DEFAULT_MAX_BYTES = 1024 * 1024;
const DEFAULT_MIN_INTERVAL_MS = 100;

interface ProbeLaunch {
  sessionId: string;
  token: string;
}

interface ProbeClient {
  socket: net.Socket;
  buffer: string;
  sessionId?: string;
  authenticated: boolean;
}

export interface ProbeSnapshotEnvelope {
  type: 'probe_snapshot';
  provider: string;
  schemaVersion: number;
  frame: number;
  simulationTime: number;
  monotonicTimestamp: number;
  snapshot: unknown;
}

export function validateProbeSnapshot(value: unknown, maxBytes = DEFAULT_MAX_BYTES): ProbeSnapshotEnvelope {
  const encoded = JSON.stringify(value);
  if (Buffer.byteLength(encoded, 'utf8') > maxBytes) throw new Error(`Probe snapshot exceeds ${maxBytes} bytes`);
  if (typeof value !== 'object' || value === null || Array.isArray(value)) throw new Error('Probe snapshot must be an object');
  const envelope = value as Partial<ProbeSnapshotEnvelope>;
  if (envelope.type !== 'probe_snapshot') throw new Error('Probe message type must be probe_snapshot');
  if (typeof envelope.provider !== 'string' || !envelope.provider.trim()) throw new Error('Probe provider is required');
  if (!Number.isInteger(envelope.schemaVersion) || Number(envelope.schemaVersion) < 1) throw new Error('Probe schemaVersion must be a positive integer');
  if (!Number.isInteger(envelope.frame) || Number(envelope.frame) < 0) throw new Error('Probe frame must be a non-negative integer');
  if (typeof envelope.simulationTime !== 'number' || !Number.isFinite(envelope.simulationTime)) throw new Error('Probe simulationTime must be finite');
  if (typeof envelope.monotonicTimestamp !== 'number' || !Number.isFinite(envelope.monotonicTimestamp)) throw new Error('Probe monotonicTimestamp must be finite');
  return envelope as ProbeSnapshotEnvelope;
}

export class RuntimeProbeServer {
  private server?: net.Server;
  private port?: number;
  private readonly launches = new Map<string, ProbeLaunch>();
  private readonly clients = new Set<ProbeClient>();
  private readonly lastSnapshotAt = new Map<string, number>();
  private rejected = 0;
  private rateLimited = 0;

  constructor(
    private readonly onSnapshot: (snapshot: ProbeSnapshotEnvelope, context: DebugCorrelationContext) => void,
    private readonly maxBytes = DEFAULT_MAX_BYTES,
    private readonly minIntervalMs = DEFAULT_MIN_INTERVAL_MS
  ) {}

  async prepareLaunch(sessionId: string): Promise<{ runtimePort: number; runtimeToken: string }> {
    await this.ensureListening();
    const token = randomBytes(32).toString('hex');
    this.launches.set(token, { sessionId, token });
    return { runtimePort: this.port as number, runtimeToken: token };
  }

  revokeSession(sessionId: string): void {
    for (const [token, launch] of this.launches) {
      if (launch.sessionId === sessionId) this.launches.delete(token);
    }
    for (const client of this.clients) {
      if (client.sessionId === sessionId) client.socket.destroy();
    }
  }

  health(): Record<string, unknown> {
    return {
      listening: Boolean(this.server),
      address: this.server ? '127.0.0.1' : null,
      port: this.port ?? null,
      authorizedLaunches: this.launches.size,
      connectedAgents: Array.from(this.clients).filter((client) => client.authenticated).length,
      maxSnapshotBytes: this.maxBytes,
      maxRateHz: Math.round(1000 / this.minIntervalMs),
      rejected: this.rejected,
      rateLimited: this.rateLimited
    };
  }

  private async ensureListening(): Promise<void> {
    if (this.server && this.port) return;
    const server = net.createServer((socket) => this.accept(socket));
    await new Promise<void>((resolve, reject) => {
      server.once('error', reject);
      server.listen(0, '127.0.0.1', () => resolve());
    });
    server.unref();
    this.server = server;
    const address = server.address();
    if (!address || typeof address === 'string') throw new Error('Runtime probe server did not receive a TCP port');
    this.port = address.port;
  }

  private accept(socket: net.Socket): void {
    if (socket.remoteAddress !== '127.0.0.1' && socket.remoteAddress !== '::ffff:127.0.0.1' && socket.remoteAddress !== '::1') {
      this.rejected++;
      socket.destroy();
      return;
    }
    socket.setEncoding('utf8');
    socket.unref();
    const client: ProbeClient = { socket, buffer: '', authenticated: false };
    this.clients.add(client);
    const authTimeout = setTimeout(() => socket.destroy(), 5_000);
    authTimeout.unref();
    socket.on('data', (chunk: string) => {
      client.buffer += chunk;
      if (Buffer.byteLength(client.buffer, 'utf8') > this.maxBytes + 16_384) {
        this.rejected++;
        socket.destroy();
        return;
      }
      let newline = client.buffer.indexOf('\n');
      while (newline >= 0) {
        const line = client.buffer.slice(0, newline).trim();
        client.buffer = client.buffer.slice(newline + 1);
        if (line) this.onLine(client, line, authTimeout);
        newline = client.buffer.indexOf('\n');
      }
    });
    socket.on('close', () => {
      clearTimeout(authTimeout);
      this.clients.delete(client);
    });
    socket.on('error', () => this.clients.delete(client));
  }

  private onLine(client: ProbeClient, line: string, authTimeout: NodeJS.Timeout): void {
    let value: unknown;
    try { value = JSON.parse(line); } catch { this.rejectClient(client); return; }
    if (!client.authenticated) {
      const hello = typeof value === 'object' && value !== null ? value as Record<string, unknown> : {};
      const launch = typeof hello.token === 'string' ? this.launches.get(hello.token) : undefined;
      if (hello.type !== 'probe_hello' || !launch || hello.sessionId !== launch.sessionId) {
        this.rejectClient(client);
        return;
      }
      clearTimeout(authTimeout);
      client.authenticated = true;
      client.sessionId = launch.sessionId;
      this.launches.delete(launch.token);
      client.socket.write(`${JSON.stringify({ type: 'probe_ack', success: true })}\n`);
      return;
    }
    try {
      const snapshot = validateProbeSnapshot(value, this.maxBytes);
      const rateKey = `${client.sessionId}:${snapshot.provider}`;
      const now = Date.now();
      const previous = this.lastSnapshotAt.get(rateKey) ?? 0;
      if (now - previous < this.minIntervalMs) {
        this.rateLimited++;
        return;
      }
      this.lastSnapshotAt.set(rateKey, now);
      this.onSnapshot(snapshot, {
        traceId: `probe:${client.sessionId}:${snapshot.provider}`,
        debugSessionId: client.sessionId,
        frame: snapshot.frame,
        timestamp: new Date().toISOString()
      });
    } catch {
      this.rejected++;
    }
  }

  private rejectClient(client: ProbeClient): void {
    this.rejected++;
    client.socket.destroy();
  }
}
