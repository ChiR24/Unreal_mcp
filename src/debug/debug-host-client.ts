import { EventEmitter } from 'node:events';
import { promises as fs } from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { Logger } from '../utils/logging/logger.js';

interface HostDiscovery {
  pipeName: string;
  token: string;
  pid: number;
  updatedAt: string;
}

interface PendingHostRequest {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
  timeout: NodeJS.Timeout;
}

interface HostMessage {
  kind?: string;
  id?: string;
  success?: boolean;
  result?: unknown;
  error?: string;
  event?: string;
  sessionId?: string;
  payload?: unknown;
}

export class DebugHostUnavailableError extends Error {
  readonly code = 'DEBUG_HOST_UNAVAILABLE';
}

export class DebugHostClient extends EventEmitter {
  private readonly log = new Logger('DebugHostClient');
  private socket?: net.Socket;
  private buffer = '';
  private connecting?: Promise<void>;
  private readonly pending = new Map<string, PendingHostRequest>();
  private discovery?: HostDiscovery;
  readonly discoveryPath: string;

  constructor(projectPath = process.env.UE_PROJECT_PATH ?? process.cwd()) {
    super();
    const projectRoot = path.extname(projectPath).toLowerCase() === '.uproject'
      ? path.dirname(path.resolve(projectPath))
      : path.resolve(projectPath);
    this.discoveryPath = path.join(projectRoot, 'Saved', 'McpDebug', 'debug-host.json');
  }

  isConnected(): boolean {
    return Boolean(this.socket && !this.socket.destroyed);
  }

  status(): Record<string, unknown> {
    return {
      connected: this.isConnected(),
      hostPid: this.discovery?.pid,
      discoveryPath: this.discoveryPath,
      setup: 'Open the Unreal project workspace in VS Code and enable the unreal-mcp-debug-host extension.'
    };
  }

  async request(command: string, argumentsValue: Record<string, unknown> = {}, timeoutMs = 15_000): Promise<unknown> {
    await this.ensureConnected();
    return this.send(command, argumentsValue, timeoutMs);
  }

  async close(): Promise<void> {
    this.socket?.destroy();
    this.socket = undefined;
    this.failPending(new DebugHostUnavailableError('Debug host connection closed'));
  }

  private async ensureConnected(): Promise<void> {
    if (this.isConnected()) return;
    if (this.connecting) return this.connecting;
    this.connecting = this.connect();
    try {
      await this.connecting;
    } finally {
      this.connecting = undefined;
    }
  }

  private async connect(): Promise<void> {
    let discovery: HostDiscovery;
    try {
      discovery = JSON.parse(await fs.readFile(this.discoveryPath, 'utf8')) as HostDiscovery;
    } catch {
      throw new DebugHostUnavailableError(`VS Code debug host discovery file not found at ${this.discoveryPath}`);
    }
    if (!discovery.pipeName || !discovery.token || !Number.isInteger(discovery.pid)) {
      throw new DebugHostUnavailableError('VS Code debug host discovery file is invalid');
    }
    this.discovery = discovery;
    const socket = net.createConnection(discovery.pipeName);
    await new Promise<void>((resolve, reject) => {
      const onError = (error: Error): void => {
        socket.off('connect', onConnect);
        reject(new DebugHostUnavailableError(`Unable to connect to VS Code debug host: ${error.message}`));
      };
      const onConnect = (): void => {
        socket.off('error', onError);
        resolve();
      };
      socket.once('error', onError);
      socket.once('connect', onConnect);
    });
    this.socket = socket;
    socket.setEncoding('utf8');
    socket.on('data', (chunk: string) => this.onData(chunk));
    socket.on('error', (error) => this.onDisconnect(error));
    socket.on('close', () => this.onDisconnect(new DebugHostUnavailableError('VS Code debug host disconnected')));
    await this.send('hello', { token: discovery.token }, 5_000);
  }

  private send(command: string, argumentsValue: Record<string, unknown>, timeoutMs: number): Promise<unknown> {
    const socket = this.socket;
    if (!socket || socket.destroyed) {
      return Promise.reject(new DebugHostUnavailableError('VS Code debug host is not connected'));
    }
    const id = randomUUID();
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Debug host request '${command}' timed out after ${timeoutMs}ms`));
      }, timeoutMs);
      this.pending.set(id, { resolve, reject, timeout });
      socket.write(`${JSON.stringify({ kind: 'request', id, command, arguments: argumentsValue })}\n`);
    });
  }

  private onData(chunk: string): void {
    this.buffer += chunk;
    let newline = this.buffer.indexOf('\n');
    while (newline >= 0) {
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      if (line) this.onMessage(line);
      newline = this.buffer.indexOf('\n');
    }
  }

  private onMessage(line: string): void {
    let message: HostMessage;
    try {
      message = JSON.parse(line) as HostMessage;
    } catch (error) {
      this.log.warn('Ignoring malformed debug host message', error instanceof Error ? error.message : String(error));
      return;
    }
    if (message.kind === 'response' && message.id) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      clearTimeout(pending.timeout);
      if (message.success === false) pending.reject(new Error(message.error ?? 'Debug host request failed'));
      else pending.resolve(message.result);
      return;
    }
    if (message.kind === 'event' && message.event) {
      this.emit('event', message);
    }
  }

  private onDisconnect(error: Error): void {
    if (!this.socket) return;
    this.socket = undefined;
    this.failPending(error);
    this.emit('disconnected', error);
  }

  private failPending(error: Error): void {
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timeout);
      pending.reject(error);
    }
    this.pending.clear();
  }
}
