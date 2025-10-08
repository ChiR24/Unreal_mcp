import { EventEmitter } from 'node:events';
import type { IncomingMessage } from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';
import { Logger } from './utils/logger.js';
import { randomUUID } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const packageInfo: { name?: string; version?: string } = (() => {
  try {
    return require('../package.json');
  } catch (error) {
    const log = new Logger('AutomationBridge');
    log.debug('Unable to read package.json for version info', error);
    return {};
  }
})();

export interface AutomationBridgeOptions {
  host?: string;
  port?: number;
  ports?: number[];
  protocols?: string[];
  capabilityToken?: string;
  enabled?: boolean;
  serverName?: string;
  serverVersion?: string;
  heartbeatIntervalMs?: number;
  maxPendingRequests?: number;
}

export interface AutomationBridgeMessage {
  type: string;
  [key: string]: unknown;
}

export interface AutomationBridgeResponseMessage extends AutomationBridgeMessage {
  requestId: string;
  success?: boolean;
  message?: string;
  error?: string;
  result?: unknown;
}

export interface AutomationBridgeStatus {
  enabled: boolean;
  host: string;
  port: number;
  configuredPorts: number[];
  listeningPorts: number[];
  connected: boolean;
  connectedAt: string | null;
  activePort: number | null;
  negotiatedProtocol: string | null;
  supportedProtocols: string[];
  supportedOpcodes: string[];
  expectedResponseOpcodes: string[];
  capabilityTokenRequired: boolean;
  lastHandshakeAt: string | null;
  lastHandshakeMetadata: Record<string, unknown> | null;
  lastHandshakeAck: Record<string, unknown> | null;
  lastHandshakeFailure: { reason: string; at: string } | null;
  lastDisconnect: { code: number; reason: string; at: string } | null;
  lastError: { message: string; at: string } | null;
  lastMessageAt: string | null;
  lastRequestSentAt: string | null;
  pendingRequests: number;
  pendingRequestDetails: Array<{ requestId: string; action: string; ageMs: number }>;
  webSocketListening: boolean;
}

type PendingRequest = {
  resolve: (value: AutomationBridgeResponseMessage) => void;
  reject: (reason: Error) => void;
  timeout: NodeJS.Timeout;
  action: string;
  requestedAt: Date;
};

type AutomationBridgeEvents = {
  connected: (info: { socket: WebSocket; metadata: Record<string, unknown>; port: number; protocol: string | null }) => void;
  disconnected: (info: { code: number; reason: string; port: number; protocol: string | null }) => void;
  message: (message: AutomationBridgeMessage) => void;
  error: (error: Error & { port?: number }) => void;
  handshakeFailed: (info: { reason: string; port: number }) => void;
};

const log = new Logger('AutomationBridge');

export class AutomationBridge extends EventEmitter {
  private readonly host: string;
  private readonly port: number;
  private readonly ports: number[];
  private readonly negotiatedProtocols: string[];
  private readonly capabilityToken?: string;
  private readonly enabled: boolean;
  private readonly wsServers = new Map<number, WebSocketServer>();
  private readonly listeningPorts = new Set<number>();
  private readonly DEFAULT_SUPPORTED_OPCODES = ['automation_request'];
  private readonly DEFAULT_RESPONSE_OPCODES = ['automation_response'];
  private readonly DEFAULT_CAPABILITIES = ['python', 'console_commands'];
  private readonly DEFAULT_HEARTBEAT_INTERVAL_MS = 30000;
  private readonly DEFAULT_MAX_PENDING_REQUESTS = 32;
  private readonly sessionId = randomUUID();
  private readonly serverName: string;
  private readonly serverVersion: string;
  private readonly heartbeatIntervalMs: number;
  private readonly maxPendingRequests: number;
  private activeSocket?: WebSocket;
  private activePort?: number;
  private activeProtocol?: string;
  private connectedAt?: Date;
  private requestCounter = 0;
  private readonly pendingRequests = new Map<string, PendingRequest>();
  private lastHandshakeAt?: Date;
  private lastHandshakeMetadata?: Record<string, unknown>;
  private lastHandshakeAck?: AutomationBridgeMessage;
  private lastHandshakeFailure?: { reason: string; at: Date };
  private lastDisconnect?: { code: number; reason: string; at: Date };
  private lastError?: { message: string; at: Date };
  private lastMessageAt?: Date;
  private lastRequestSentAt?: Date;
  private heartbeatTimer?: NodeJS.Timeout;

  constructor(options: AutomationBridgeOptions = {}) {
    super();
    this.host = options.host ?? process.env.MCP_AUTOMATION_WS_HOST ?? '127.0.0.1';
    const sanitizePort = (value: unknown): number | null => {
      if (typeof value === 'number' && Number.isInteger(value)) {
        return value > 0 && value <= 65535 ? value : null;
      }
      if (typeof value === 'string' && value.trim().length > 0) {
        const parsed = Number.parseInt(value.trim(), 10);
        return Number.isInteger(parsed) && parsed > 0 && parsed <= 65535 ? parsed : null;
      }
      return null;
    };

    const fallbackPort = sanitizePort(options.port ?? process.env.MCP_AUTOMATION_WS_PORT) ?? 8090;
    const configuredPortValues: Array<number | string> | undefined = options.ports
      ? options.ports
      : process.env.MCP_AUTOMATION_WS_PORTS
        ?.split(',')
        .map((token) => token.trim())
        .filter((token) => token.length > 0);

    const sanitizedPorts = Array.isArray(configuredPortValues)
      ? configuredPortValues
          .map((value) => sanitizePort(value))
          .filter((port): port is number => port !== null)
      : [];

    if (!sanitizedPorts.includes(fallbackPort)) {
      sanitizedPorts.unshift(fallbackPort);
    }
    if (sanitizedPorts.length === 0) {
      sanitizedPorts.push(8090);
    }

    this.ports = Array.from(new Set(sanitizedPorts));
    const defaultProtocols = ['mcp-automation'];
    const userProtocols = Array.isArray(options.protocols)
      ? options.protocols.filter((proto) => typeof proto === 'string' && proto.trim().length > 0)
      : [];
    const envProtocols = process.env.MCP_AUTOMATION_WS_PROTOCOLS
      ? process.env.MCP_AUTOMATION_WS_PROTOCOLS.split(',')
          .map((token) => token.trim())
          .filter((token) => token.length > 0)
      : [];
    this.negotiatedProtocols = Array.from(new Set([...userProtocols, ...envProtocols, ...defaultProtocols]));
    this.port = this.ports[0];
    this.capabilityToken =
      options.capabilityToken ?? process.env.MCP_AUTOMATION_CAPABILITY_TOKEN ?? undefined;
    this.enabled = options.enabled ?? process.env.MCP_AUTOMATION_BRIDGE_ENABLED !== 'false';
    this.serverName = options.serverName
      ?? process.env.MCP_SERVER_NAME
      ?? packageInfo.name
      ?? 'unreal-engine-mcp';
    this.serverVersion = options.serverVersion
      ?? process.env.MCP_SERVER_VERSION
      ?? packageInfo.version
      ?? process.env.npm_package_version
      ?? '0.0.0';
    const resolvedHeartbeat = options.heartbeatIntervalMs ?? this.DEFAULT_HEARTBEAT_INTERVAL_MS;
    this.heartbeatIntervalMs = resolvedHeartbeat > 0 ? resolvedHeartbeat : 0;
    const resolvedMaxPending = options.maxPendingRequests ?? this.DEFAULT_MAX_PENDING_REQUESTS;
    this.maxPendingRequests = Math.max(1, resolvedMaxPending);
  }

  override on<K extends keyof AutomationBridgeEvents>(
    event: K,
    listener: AutomationBridgeEvents[K]
  ): this {
    return super.on(event, listener as (...args: unknown[]) => void);
  }

  override once<K extends keyof AutomationBridgeEvents>(
    event: K,
    listener: AutomationBridgeEvents[K]
  ): this {
    return super.once(event, listener as (...args: unknown[]) => void);
  }

  override off<K extends keyof AutomationBridgeEvents>(
    event: K,
    listener: AutomationBridgeEvents[K]
  ): this {
    return super.off(event, listener as (...args: unknown[]) => void);
  }

  /** Starts the WebSocket server if enabled. */
  start(): void {
    if (!this.enabled) {
      log.info('Automation bridge disabled by configuration.');
      return;
    }
    const targetPorts = this.ports.length > 0 ? this.ports : [this.port];

    for (const port of targetPorts) {
      if (this.wsServers.has(port)) {
        continue;
      }

      try {
        const server = new WebSocketServer({
          host: this.host,
          port,
          handleProtocols: (protocols: Set<string>): string | false => {
            const offered = Array.from(protocols);
            for (const preferred of this.negotiatedProtocols) {
              if (protocols.has(preferred)) {
                return preferred;
              }
            }
            return offered[0] ?? false;
          },
          perMessageDeflate: false
        });
  this.wsServers.set(port, server);

  server.on('connection', (socket: WebSocket, request: IncomingMessage) => {
          const remote = request.socket.remoteAddress ?? 'unknown';
          log.info(`Automation bridge client connected from ${remote} on port ${port}`);
          this.handleConnection(socket, port);
        });

        server.on('error', (err: unknown) => {
          const errorObj = err instanceof Error ? err : new Error(String(err));
          this.lastError = { message: errorObj.message, at: new Date() };
          log.error(`Automation bridge WebSocket server error on port ${port}`, errorObj);
          const errorWithPort = Object.assign(errorObj, { port });
          this.emitAutomation('error', errorWithPort);
          this.listeningPorts.delete(port);
          this.wsServers.delete(port);
        });

        server.on('listening', () => {
          this.listeningPorts.add(port);
          log.info(`Automation bridge listening on ws://${this.host}:${port}`);
        });

        server.on('close', () => {
          this.listeningPorts.delete(port);
          this.wsServers.delete(port);
        });
      } catch (error) {
        const errorObj = error instanceof Error ? error : new Error(String(error));
        this.lastError = { message: errorObj.message, at: new Date() };
        log.error(`Automation bridge failed to listen on port ${port}`, errorObj);
        const errorWithPort = Object.assign(errorObj, { port });
        this.emitAutomation('error', errorWithPort);
      }
    }

    if (this.wsServers.size === 0) {
      log.warn('Automation bridge could not start any WebSocket listeners.');
    }
  }

  /** Stops the WebSocket server and active connection. */
  stop(): void {
    if (this.isConnected()) {
      this.send({
        type: 'bridge_shutdown',
        sessionId: this.sessionId,
        timestamp: new Date().toISOString(),
        reason: 'Server shutting down'
      });
    }
    this.stopHeartbeat();
    if (this.activeSocket) {
      this.activeSocket.removeAllListeners();
      this.activeSocket.close(1001, 'Server shutdown');
      this.activeSocket = undefined;
    }
    for (const server of this.wsServers.values()) {
      try {
        server.close();
      } catch (error) {
        const errObj = error instanceof Error ? error : new Error(String(error));
        log.warn('Automation bridge error while closing WebSocket server', errObj);
      }
    }
    this.wsServers.clear();
    this.listeningPorts.clear();
    this.connectedAt = undefined;
    this.activePort = undefined;
    this.activeProtocol = undefined;
    this.lastHandshakeAck = undefined;
    this.rejectAllPending(new Error('Automation bridge server stopped'));
  }

  /** Returns whether the automation bridge has an active, authenticated socket. */
  isConnected(): boolean {
    return Boolean(this.activeSocket && this.activeSocket.readyState === WebSocket.OPEN);
  }

  /** Returns diagnostic metadata for health endpoints. */
  getStatus(): AutomationBridgeStatus {
    const now = Date.now();
    const pendingSummaries = Array.from(this.pendingRequests.entries()).map(([requestId, pending]) => ({
      requestId,
      action: pending.action,
      ageMs: Math.max(0, now - pending.requestedAt.getTime())
    }));

    return {
      enabled: this.enabled,
      host: this.host,
      port: this.port,
      configuredPorts: [...this.ports],
      listeningPorts: Array.from(this.listeningPorts.values()),
      connected: this.isConnected(),
      connectedAt: this.connectedAt?.toISOString() ?? null,
      activePort: this.activePort ?? null,
      negotiatedProtocol: this.activeProtocol ?? null,
      supportedProtocols: [...this.negotiatedProtocols],
      supportedOpcodes: [...this.DEFAULT_SUPPORTED_OPCODES],
      expectedResponseOpcodes: [...this.DEFAULT_RESPONSE_OPCODES],
      capabilityTokenRequired: Boolean(this.capabilityToken),
      lastHandshakeAt: this.lastHandshakeAt?.toISOString() ?? null,
      lastHandshakeMetadata: this.lastHandshakeMetadata ?? null,
      lastHandshakeAck: this.lastHandshakeAck ?? null,
      lastHandshakeFailure: this.lastHandshakeFailure
        ? { reason: this.lastHandshakeFailure.reason, at: this.lastHandshakeFailure.at.toISOString() }
        : null,
      lastDisconnect: this.lastDisconnect
        ? { code: this.lastDisconnect.code, reason: this.lastDisconnect.reason, at: this.lastDisconnect.at.toISOString() }
        : null,
      lastError: this.lastError
        ? { message: this.lastError.message, at: this.lastError.at.toISOString() }
        : null,
      lastMessageAt: this.lastMessageAt?.toISOString() ?? null,
      lastRequestSentAt: this.lastRequestSentAt?.toISOString() ?? null,
      pendingRequests: this.pendingRequests.size,
      pendingRequestDetails: pendingSummaries,
      webSocketListening: this.listeningPorts.size > 0
    };
  }

  /** Sends a JSON-serializable payload to the plugin if connected. */
  send(payload: AutomationBridgeMessage): boolean {
    if (!this.isConnected()) {
      log.warn('Attempted to send automation message without an active connection');
      return false;
    }
    const socket = this.activeSocket;
    if (!socket) {
      log.warn('Automation bridge socket disappeared before send');
      return false;
    }
    try {
      socket.send(JSON.stringify(payload));
      return true;
    } catch (error) {
      log.error('Failed to send automation message', error);
      const errObj = error instanceof Error ? error : new Error(String(error));
      const errorWithPort = Object.assign(errObj, { port: this.activePort });
      this.emitAutomation('error', errorWithPort);
      return false;
    }
  }

  private handleConnection(socket: WebSocket, port: number): void {
    if (this.activeSocket && this.activeSocket.readyState === WebSocket.OPEN) {
      const previousPort = this.activePort;
      log.warn(
        `Existing automation bridge connection detected on port ${previousPort ?? 'unknown'}; closing previous socket.`
      );
      this.stopHeartbeat();
      this.activeSocket.close(4001, 'Superseded by new connection');
      this.rejectAllPending(new Error('Automation bridge connection superseded by new client.'));
    }

    let handshakeComplete = false;
    const handshakeTimeout = setTimeout(() => {
      if (!handshakeComplete) {
        log.warn('Automation bridge handshake timed out; closing connection.');
        socket.close(4002, 'Handshake timeout');
        this.lastHandshakeFailure = { reason: 'timeout', at: new Date() };
        this.emitAutomation('handshakeFailed', { reason: 'timeout', port });
      }
    }, 5000);

    socket.on('message', (data) => {
      let parsed: AutomationBridgeMessage;
      try {
        const text = typeof data === 'string' ? data : data.toString('utf8');
        parsed = JSON.parse(text);
      } catch (error) {
        log.error('Received non-JSON automation message; closing connection.', error);
        socket.close(4003, 'Invalid JSON payload');
        this.lastHandshakeFailure = { reason: 'invalid-json', at: new Date() };
        this.emitAutomation('handshakeFailed', { reason: 'invalid-json', port });
        return;
      }

      if (!handshakeComplete) {
        if (parsed.type !== 'bridge_hello') {
          log.warn(`Expected bridge_hello handshake, received ${parsed.type}; closing.`);
          socket.close(4004, 'Handshake expected bridge_hello');
          this.lastHandshakeFailure = { reason: 'invalid-handshake', at: new Date() };
          this.emitAutomation('handshakeFailed', { reason: 'invalid-handshake', port });
          return;
        }

        if (this.capabilityToken && parsed.capabilityToken !== this.capabilityToken) {
          log.warn('Automation bridge capability token mismatch; closing connection.');
          socket.send(
            JSON.stringify({ type: 'bridge_error', error: 'INVALID_CAPABILITY_TOKEN' })
          );
          socket.close(4005, 'Invalid capability token');
          this.lastHandshakeFailure = { reason: 'invalid-token', at: new Date() };
          this.emitAutomation('handshakeFailed', { reason: 'invalid-token', port });
          return;
        }

        handshakeComplete = true;
        clearTimeout(handshakeTimeout);
        const now = new Date();
        this.registerSocket(socket, port);
        this.lastHandshakeAt = now;
        const metadata = this.sanitizeHandshakeMetadata(parsed as Record<string, unknown>);
        this.lastHandshakeMetadata = metadata;
        this.lastHandshakeFailure = undefined;
        this.lastMessageAt = now;
  const ackPayload = this.createHandshakeAck(parsed as Record<string, unknown>);
        this.lastHandshakeAck = ackPayload;
    this.send(ackPayload);
    this.startHeartbeat();
        this.emitAutomation('connected', {
          socket,
          metadata,
          port,
          protocol: socket.protocol || null
        });
        return;
      }

      this.handleAutomationMessage(parsed);
      this.lastMessageAt = new Date();
      this.emitAutomation('message', parsed);
    });

    socket.on('error', (error) => {
      log.error('Automation bridge socket error', error);
      const errObj = error instanceof Error ? error : new Error(String(error));
      this.lastError = { message: errObj.message, at: new Date() };
      const errWithPort = Object.assign(errObj, { port });
      this.emitAutomation('error', errWithPort);
    });

    socket.on('close', (code, reasonBuffer) => {
      const reason = reasonBuffer.toString('utf8');
      if (socket === this.activeSocket) {
        this.activeSocket = undefined;
        this.connectedAt = undefined;
        this.activePort = undefined;
      }
      clearTimeout(handshakeTimeout);
      this.stopHeartbeat();
      this.lastDisconnect = { code, reason, at: new Date() };
      this.emitAutomation('disconnected', {
        code,
        reason,
        port,
        protocol: socket.protocol || this.activeProtocol || null
      });
      log.info(`Automation bridge socket closed (code=${code}, reason=${reason})`);
      this.rejectAllPending(new Error(`Automation bridge connection closed (${code}): ${reason || 'no reason'}`));
    });
  }

  private registerSocket(socket: WebSocket, port: number) {
    this.activeSocket = socket;
    this.activePort = port;
    this.activeProtocol = socket.protocol || undefined;
    this.connectedAt = new Date();
  }

  private sanitizeHandshakeMetadata(payload: Record<string, unknown>): Record<string, unknown> {
    const sanitized: Record<string, unknown> = { ...payload };
    delete sanitized.type;
    if ('capabilityToken' in sanitized) {
      sanitized.capabilityToken = 'REDACTED';
    }
    return sanitized;
  }

  private createHandshakeAck(handshake: Record<string, unknown>): AutomationBridgeMessage {
    const handshakeVersionRaw =
      handshake['protocolVersion'] ?? handshake['protocol_version'] ?? 1;
    const protocolVersion =
      typeof handshakeVersionRaw === 'number' || typeof handshakeVersionRaw === 'string'
        ? handshakeVersionRaw
        : 1;

    return {
      type: 'bridge_ack',
      message: 'Automation bridge ready',
      serverName: this.serverName,
      serverVersion: this.serverVersion,
      sessionId: this.sessionId,
      protocolVersion,
      supportedOpcodes: [...this.DEFAULT_SUPPORTED_OPCODES],
      expectedResponseOpcodes: [...this.DEFAULT_RESPONSE_OPCODES],
      capabilities: [...this.DEFAULT_CAPABILITIES],
      heartbeatIntervalMs: this.heartbeatIntervalMs,
      maxPendingRequests: this.maxPendingRequests,
      supportedProtocols: [...this.negotiatedProtocols],
      availablePorts: [...this.ports],
      activePort: this.activePort ?? null,
      protocol: this.activeProtocol ?? null
    };
  }

  private handleAutomationMessage(message: AutomationBridgeMessage): void {
    switch (message.type) {
      case 'automation_response':
        this.handleAutomationResponse(message as AutomationBridgeResponseMessage);
        break;
      case 'bridge_ping':
        this.handleBridgePing(message);
        break;
      case 'bridge_goodbye':
        log.info('Automation bridge client initiated shutdown.', message);
        break;
      default:
        log.debug('Received automation bridge message with no handler', message);
        break;
    }
  }

  private handleAutomationResponse(response: AutomationBridgeResponseMessage): void {
    const requestId = response.requestId;
    if (!requestId) {
      log.warn('Received automation_response without requestId');
      return;
    }

    const pending = this.pendingRequests.get(requestId);
    if (!pending) {
      log.warn(`No pending automation request found for requestId=${requestId}`);
      return;
    }

    clearTimeout(pending.timeout);
    this.pendingRequests.delete(requestId);
    pending.resolve(response);
  }

  private handleBridgePing(message: AutomationBridgeMessage): void {
    const pong: AutomationBridgeMessage = {
      type: 'bridge_pong',
      sessionId: this.sessionId,
      timestamp: new Date().toISOString()
    };

    if (Object.prototype.hasOwnProperty.call(message, 'nonce')) {
      pong.nonce = message.nonce;
    }

    this.send(pong);
  }

  private startHeartbeat(): void {
    this.stopHeartbeat();
    if (this.heartbeatIntervalMs <= 0) {
      return;
    }

    this.heartbeatTimer = setInterval(() => {
      if (!this.isConnected()) {
        this.stopHeartbeat();
        return;
      }

      this.send({
        type: 'bridge_heartbeat',
        sessionId: this.sessionId,
        timestamp: new Date().toISOString()
      });
    }, this.heartbeatIntervalMs);
  }

  private stopHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = undefined;
    }
  }

  private rejectAllPending(error: Error): void {
    if (this.pendingRequests.size === 0) {
      return;
    }

    for (const [requestId, pending] of [...this.pendingRequests.entries()]) {
      clearTimeout(pending.timeout);
      pending.reject(error);
      this.pendingRequests.delete(requestId);
    }
  }

  private generateRequestId(): string {
    this.requestCounter = (this.requestCounter + 1) % Number.MAX_SAFE_INTEGER;
    return `req-${Date.now().toString(36)}-${this.requestCounter.toString(36)}`;
  }

  public async sendAutomationRequest(
    action: string,
    payload: Record<string, unknown> = {},
    options: { timeoutMs?: number } = {}
  ): Promise<AutomationBridgeResponseMessage> {
    if (!this.enabled) {
      throw new Error('Automation bridge disabled');
    }
    if (!this.isConnected()) {
      throw new Error('Automation bridge not connected');
    }

    const timeoutMs = Math.max(1000, options.timeoutMs ?? 15000);
    const requestId = this.generateRequestId();

    const requestPayload: AutomationBridgeMessage = {
      type: 'automation_request',
      requestId,
      action,
      payload
    };

    const pendingPromise = new Promise<AutomationBridgeResponseMessage>((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingRequests.delete(requestId);
        reject(new Error(`Automation request timed out after ${timeoutMs}ms`));
      }, timeoutMs);

      this.pendingRequests.set(requestId, {
        resolve,
        reject,
        timeout,
        action,
        requestedAt: new Date()
      });
    });

    this.lastRequestSentAt = new Date();
    const sent = this.send(requestPayload);
    if (!sent) {
      const pending = this.pendingRequests.get(requestId);
      if (pending) {
        clearTimeout(pending.timeout);
        this.pendingRequests.delete(requestId);
      }
      throw new Error('Failed to dispatch automation request');
    }

    return pendingPromise;
  }

  private emitAutomation<K extends keyof AutomationBridgeEvents>(
    event: K,
    payload: Parameters<AutomationBridgeEvents[K]>[0]
  ): void {
    this.emit(event, payload);
  }
}

export type AutomationBridgeEvent = keyof AutomationBridgeEvents;