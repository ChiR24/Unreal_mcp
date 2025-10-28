import { EventEmitter } from 'node:events';
import type { IncomingMessage } from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';
import { Logger } from './utils/logger.js';
import {
  DEFAULT_AUTOMATION_HOST,
  DEFAULT_AUTOMATION_PORT,
  DEFAULT_NEGOTIATED_PROTOCOLS,
  DEFAULT_HEARTBEAT_INTERVAL_MS,
  DEFAULT_HANDSHAKE_TIMEOUT_MS,
  DEFAULT_MAX_PENDING_REQUESTS
} from './constants.js';
import { randomUUID } from 'node:crypto';
import { createRequire } from 'node:module';

function FStringSafe(val: unknown): string {
  try {
    if (val === undefined || val === null) return '';
    if (typeof val === 'string') return val;
    return JSON.stringify(val);
  } catch {
    try { return String(val); } catch { return ''; }
  }
}

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
  maxConcurrentConnections?: number;
  clientMode?: boolean; // New option to enable client mode
  clientHost?: string; // Host to connect to in client mode
  clientPort?: number; // Port to connect to in client mode
  serverLegacyEnabled?: boolean; // Enable legacy server listener alongside client mode
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
  /**
   * Detailed list of current active connections. Each entry includes the
   * server-side connectionId (internal), optional sessionId reported by the
   * client during handshake, and the client's remote address/port.
   */
  connections: Array<{
    connectionId: string;
    sessionId: string | null;
    remoteAddress: string | null;
    remotePort: number | null;
    port: number;
    connectedAt: string;
    protocol: string | null;
    readyState: number;
    isPrimary: boolean;
  }>;
  webSocketListening: boolean;
  serverLegacyEnabled: boolean;
  serverName: string;
  serverVersion: string;
  maxConcurrentConnections: number;
  maxPendingRequests: number;
  heartbeatIntervalMs: number;
}

type PendingRequest = {
  resolve: (value: AutomationBridgeResponseMessage) => void;
  reject: (reason: Error) => void;
  timeout: NodeJS.Timeout;
  action: string;
  requestedAt: Date;
  // If waitForEvent is true the initial automation_response will be
  // stored when received and the pending request will remain open until
  // a matching automation_event with the same requestId is delivered.
  waitForEvent?: boolean;
  // Timer used while waiting for the completion event
  eventTimeout?: NodeJS.Timeout | undefined;
  // Optional event timeout override (ms)
  eventTimeoutMs?: number | undefined;
  // Store the initial automation_response when waiting for a completion event
  initialResponse?: AutomationBridgeResponseMessage | undefined;
};

type AutomationBridgeEvents = {
  connected: (info: { socket: WebSocket; metadata: Record<string, unknown>; port: number; protocol: string | null }) => void;
  disconnected: (info: { code: number; reason: string; port: number; protocol: string | null }) => void;
  message: (message: AutomationBridgeMessage) => void;
  error: (error: Error & { port?: number }) => void;
  handshakeFailed: (info: { reason: string; port: number }) => void;
};

const log = new Logger('AutomationBridge');

// Actions that typically complete asynchronously inside the editor and should
// only be considered "done" once a completion event or follow-up response is
// observed. Enabling waitForEvent for these avoids false-positive successes
// from initial acknowledgements.
const WAIT_FOR_EVENT_ACTIONS = new Set<string>([
  // Sequencer
  'sequence_create',
  'sequence_open',
  'sequence_add_camera',
  'sequence_add_actor',
  'sequence_add_actors',
  'sequence_remove_actors',
  'sequence_set_properties',
  'sequence_set_playback_speed',
  'sequence_get_properties',
  'sequence_get_bindings',
  'sequence_add_spawnable_from_class',
  'add_sequencer_keyframe',
  'manage_sequencer_track',
  // Asset ops
  'duplicate_asset',
  'rename_asset',
  'delete_assets',
  'move_asset',
  'create_folder',
  'import_asset',
  'save_asset',
  'set_tags',
  'create_thumbnail',
  'generate_report',
  'validate',
  'fixup_redirectors',
  // Generic editor property changes
  'set_object_property',
  'execute_editor_function'
]);

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
  // Heartbeat / pending-request defaults moved to shared constants
  private readonly sessionId = randomUUID();
  private readonly serverName: string;
  private readonly serverVersion: string;
  private readonly heartbeatIntervalMs: number;
  private readonly maxPendingRequests: number;
  private readonly maxConcurrentConnections: number;
  private readonly clientMode: boolean; // New property for client mode
  private readonly clientHost: string; // Host to connect to in client mode
  private readonly clientPort: number; // Port to connect to in client mode
  private readonly serverLegacyEnabled: boolean;
  private activeSockets = new Map<
    WebSocket,
    {
      connectionId: string;
      port: number;
      connectedAt: Date;
      protocol?: string;
      sessionId?: string;
      remoteAddress?: string;
      remotePort?: number;
    }
  >();
  private primarySocket?: WebSocket; // Primary socket for sending requests
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
  // Coalescing map for identical, idempotent requests (e.g. blueprint_exists)
  private readonly coalescedRequests = new Map<string, Promise<AutomationBridgeResponseMessage>>();

  constructor(options: AutomationBridgeOptions = {}) {
    super();
    this.host = options.host ?? process.env.MCP_AUTOMATION_WS_HOST ?? DEFAULT_AUTOMATION_HOST;
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

    const defaultPort = sanitizePort(options.port ?? process.env.MCP_AUTOMATION_WS_PORT) ?? DEFAULT_AUTOMATION_PORT;
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

    if (!sanitizedPorts.includes(defaultPort)) {
      sanitizedPorts.unshift(defaultPort);
    }
    if (sanitizedPorts.length === 0) {
      sanitizedPorts.push(DEFAULT_AUTOMATION_PORT);
    }

    this.ports = Array.from(new Set(sanitizedPorts));
    const defaultProtocols = DEFAULT_NEGOTIATED_PROTOCOLS;
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
    this.serverLegacyEnabled =
      options.serverLegacyEnabled ?? process.env.MCP_AUTOMATION_SERVER_LEGACY !== 'false';
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
    const resolvedHeartbeat = options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS;
    this.heartbeatIntervalMs = resolvedHeartbeat > 0 ? resolvedHeartbeat : 0;
    const resolvedMaxPending = options.maxPendingRequests ?? DEFAULT_MAX_PENDING_REQUESTS;
    this.maxPendingRequests = Math.max(1, resolvedMaxPending);
    const resolvedMaxConnections = options.maxConcurrentConnections ?? 10; // Allow up to 10 concurrent connections
    this.maxConcurrentConnections = Math.max(1, resolvedMaxConnections);
    this.clientMode = options.clientMode ?? process.env.MCP_AUTOMATION_CLIENT_MODE === 'true';
    this.clientHost = options.clientHost ?? process.env.MCP_AUTOMATION_CLIENT_HOST ?? DEFAULT_AUTOMATION_HOST;
    this.clientPort = options.clientPort ?? sanitizePort(process.env.MCP_AUTOMATION_CLIENT_PORT) ?? DEFAULT_AUTOMATION_PORT;
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

  /** Starts the WebSocket server if enabled, or connects as client in client mode. */
  start(): void {
    if (!this.enabled) {
      log.info('Automation bridge disabled by configuration.');
      return;
    }

    // Always operate in client mode - connect to the Unreal plugin server
    log.info(`Automation bridge connecting to Unreal server at ws://${this.clientHost}:${this.clientPort}`);
    this.startClient();
  }

  /** Starts the WebSocket server (legacy server mode). */
  private startServer(): void {
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
          const remoteAddr = request.socket.remoteAddress ?? undefined;
          const remotePort = (request.socket as any).remotePort ?? undefined;
          log.info(`Automation bridge client connected from ${remoteAddr ?? 'unknown'} on port ${port}`);
          this.handleConnection(socket, port, remoteAddr, remotePort);
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

  /** Starts the WebSocket client to connect to Unreal Engine plugin server. */
  private startClient(): void {
    try {
      const url = `ws://${this.clientHost}:${this.clientPort}`;
      log.info(`Connecting to Unreal Engine automation server at ${url}`);

      const socket = new WebSocket(url, this.negotiatedProtocols, {
        headers: this.capabilityToken ? { 'X-MCP-Capability': this.capabilityToken } : undefined,
        perMessageDeflate: false
      });

      this.handleClientConnection(socket);
    } catch (error) {
      const errorObj = error instanceof Error ? error : new Error(String(error));
      this.lastError = { message: errorObj.message, at: new Date() };
      log.error('Failed to create WebSocket client connection', errorObj);
      const errorWithPort = Object.assign(errorObj, { port: this.clientPort });
      this.emitAutomation('error', errorWithPort);
    }
  }

  /** Stops the WebSocket server and active connection. */
  stop(): void {
    if (this.isConnected()) {
      this.broadcast({
        type: 'bridge_shutdown',
        sessionId: this.sessionId,
        timestamp: new Date().toISOString(),
        reason: 'Server shutting down'
      });
    }
    this.stopHeartbeat();
    for (const [socket] of this.activeSockets) {
      socket.removeAllListeners();
      socket.close(1001, 'Server shutdown');
    }
    this.activeSockets.clear();
    this.primarySocket = undefined;
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
    this.lastHandshakeAck = undefined;
    this.rejectAllPending(new Error('Automation bridge server stopped'));
  }

  /** Returns whether the automation bridge has an active, authenticated socket. */
  isConnected(): boolean {
    return this.activeSockets.size > 0;
  }

  /** Returns diagnostic metadata for health endpoints. */
  getStatus(): AutomationBridgeStatus {
    const now = Date.now();
    const pendingSummaries = Array.from(this.pendingRequests.entries()).map(([requestId, pending]) => ({
      requestId,
      action: pending.action,
      ageMs: Math.max(0, now - pending.requestedAt.getTime())
    }));

    // Get info about all active connections
    const connectionInfos = Array.from(this.activeSockets.entries()).map(([socket, info]) => ({
      connectionId: info.connectionId,
      sessionId: info.sessionId ?? null,
      remoteAddress: info.remoteAddress ?? null,
      remotePort: info.remotePort ?? null,
      port: info.port,
      connectedAt: info.connectedAt.toISOString(),
      protocol: info.protocol || null,
      readyState: socket.readyState,
      isPrimary: socket === this.primarySocket
    }));

    return {
      enabled: this.enabled,
      host: this.host,
      port: this.port,
      configuredPorts: [...this.ports],
      listeningPorts: Array.from(this.listeningPorts.values()),
      connected: this.isConnected(),
      connectedAt: connectionInfos.length > 0 ? connectionInfos[0].connectedAt : null,
      activePort: connectionInfos.length > 0 ? connectionInfos[0].port : null,
      negotiatedProtocol: connectionInfos.length > 0 ? connectionInfos[0].protocol : null,
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
      // Expose active connection details for admin/health purposes
      connections: connectionInfos,
      webSocketListening: this.listeningPorts.size > 0,
      serverLegacyEnabled: this.serverLegacyEnabled,
      serverName: this.serverName,
      serverVersion: this.serverVersion,
      maxConcurrentConnections: this.maxConcurrentConnections,
      maxPendingRequests: this.maxPendingRequests,
      heartbeatIntervalMs: this.heartbeatIntervalMs
    };
  }

  /** Sends a JSON-serializable payload to the primary socket if connected. */
  send(payload: AutomationBridgeMessage): boolean {
    if (!this.primarySocket || this.primarySocket.readyState !== WebSocket.OPEN) {
      log.warn('Attempted to send automation message without an active primary connection');
      return false;
    }
    try {
      this.primarySocket.send(JSON.stringify(payload));
      return true;
    } catch (error) {
      log.error('Failed to send automation message', error);
      const errObj = error instanceof Error ? error : new Error(String(error));
      const primaryInfo = this.activeSockets.get(this.primarySocket);
      const errorWithPort = Object.assign(errObj, { port: primaryInfo?.port });
      this.emitAutomation('error', errorWithPort);
      return false;
    }
  }

  /** Broadcasts a message to all connected sockets. */
  private broadcast(payload: AutomationBridgeMessage): boolean {
    if (this.activeSockets.size === 0) {
      log.warn('Attempted to broadcast automation message without any active connections');
      return false;
    }
    let sentCount = 0;
    for (const [socket] of this.activeSockets) {
      if (socket.readyState === WebSocket.OPEN) {
        try {
          socket.send(JSON.stringify(payload));
          sentCount++;
        } catch (error) {
          log.error('Failed to broadcast automation message to socket', error);
        }
      }
    }
    return sentCount > 0;
  }

  private handleConnection(socket: WebSocket, port: number, remoteAddress?: string, remotePort?: number): void {
    // Check if we've reached the maximum concurrent connections
    if (this.activeSockets.size >= this.maxConcurrentConnections) {
      log.warn(
        `Maximum concurrent connections (${this.maxConcurrentConnections}) reached; rejecting new connection from port ${port}.`
      );
      socket.close(4001, 'Maximum concurrent connections reached');
      this.emitAutomation('handshakeFailed', { reason: 'max-connections-reached', port });
      return;
    }

    let handshakeComplete = false;
    const handshakeTimeout = setTimeout(() => {
      if (!handshakeComplete) {
        log.warn('Automation bridge handshake timed out; closing connection.');
        socket.close(4002, 'Handshake timeout');
        this.lastHandshakeFailure = { reason: 'timeout', at: new Date() };
        this.emitAutomation('handshakeFailed', { reason: 'timeout', port });
      }
    }, DEFAULT_HANDSHAKE_TIMEOUT_MS);

    socket.on('message', (data, isBinary) => {
      let parsed: AutomationBridgeMessage;
        try {
        const text = typeof data === 'string' ? data : data.toString('utf8');
        parsed = JSON.parse(text);
        // For important automation messages (responses/requests/events) log
        // at info level so test runners can observe them without enabling
        // verbose debug globally.
        if (parsed && (parsed.type === 'automation_response' || parsed.type === 'automation_request' || parsed.type === 'automation_event')) {
          const rid = (parsed as any).requestId ?? 'n/a';
          const action = (parsed as any).action ?? '';
          log.info(`[AutomationBridge] Received ${parsed.type}${action ? ` action=${action}` : ''} requestId=${rid} length=${text.length}`);
        } else {
          log.debug(`[AutomationBridge] Received message (binary=${isBinary}, length=${text.length}): ${text.substring(0, 200)}`);
        }
        } catch (_error) {
          const text = typeof data === 'string' ? data : data.toString('utf8');
          log.error(`Received non-JSON automation message (binary=${isBinary}); closing connection. Data: ${text.substring(0, 500)}`, _error as any);
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
          log.warn('Automation bridge capability token mismatch; closing connection. Received token=%s, expected=%s', String(parsed.capabilityToken ?? '<none>'), '<REDACTED>');
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
        // Sanitize metadata first so we can store any client-provided sessionId
        const metadata = this.sanitizeHandshakeMetadata(parsed as Record<string, unknown>);
        this.lastHandshakeMetadata = metadata;
        // Log handshake metadata at info level (token redacted by sanitizeHandshakeMetadata)
        try {
          log.info(`Automation bridge handshake succeeded (port=${port}) metadata=${JSON.stringify(metadata)}`);
        } catch (_e) {
          log.info('Automation bridge handshake succeeded (port=%d) [metadata redaction failed]', port);
        }
        this.lastHandshakeFailure = undefined;
        this.lastMessageAt = now;
        // Register the socket with metadata and remote address information
        this.registerSocket(socket, port, metadata, remoteAddress, remotePort);
        this.lastHandshakeAt = now;
        const ackPayload = this.createHandshakeAck(parsed as Record<string, unknown>, port, socket);
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
      const socketInfo = this.activeSockets.get(socket);
      if (socketInfo) {
        this.activeSockets.delete(socket);
        // If this was the primary socket, choose a new primary
        if (socket === this.primarySocket) {
          this.primarySocket = this.activeSockets.size > 0 ? this.activeSockets.keys().next().value : undefined;
          if (this.activeSockets.size === 0) {
            this.stopHeartbeat();
          }
        }
        clearTimeout(handshakeTimeout);
        this.lastDisconnect = { code, reason, at: new Date() };
        this.emitAutomation('disconnected', {
          code,
          reason,
          port: socketInfo.port,
          protocol: socketInfo.protocol || null
        });
        log.info(`Automation bridge socket closed (code=${code}, reason=${reason})`);
        // Only reject pending requests if this was the last connection
        if (this.activeSockets.size === 0) {
          this.rejectAllPending(new Error(`Automation bridge connection closed (${code}): ${reason || 'no reason'}`));
        }
      }
    });
  }

  /** Handles client connection to Unreal Engine plugin server. */
  private handleClientConnection(socket: WebSocket): void {
    let handshakeComplete = false;
    const handshakeTimeout = setTimeout(() => {
      if (!handshakeComplete) {
        log.warn('Automation bridge client handshake timed out; closing connection.');
        socket.close(4002, 'Handshake timeout');
        this.lastHandshakeFailure = { reason: 'timeout', at: new Date() };
        this.emitAutomation('handshakeFailed', { reason: 'timeout', port: this.clientPort });
      }
    }, DEFAULT_HANDSHAKE_TIMEOUT_MS);

    socket.on('open', () => {
      log.info('Automation bridge client connected, sending handshake');
      // Send bridge_hello to initiate handshake
      const helloPayload: AutomationBridgeMessage = {
        type: 'bridge_hello',
        capabilityToken: this.capabilityToken || undefined
      };
      socket.send(JSON.stringify(helloPayload));
    });

    socket.on('message', (data, isBinary) => {
      let parsed: AutomationBridgeMessage;
      try {
        const text = typeof data === 'string' ? data : data.toString('utf8');
        log.debug(`[AutomationBridge Client] Received message (binary=${isBinary}, length=${text.length}): ${text.substring(0, 200)}`);
        parsed = JSON.parse(text);
      } catch (_error) {
        const text = typeof data === 'string' ? data : data.toString('utf8');
        log.error(`Received non-JSON automation message from server (binary=${isBinary}); closing connection. Data: ${text.substring(0, 500)}`, _error as any);
        socket.close(4003, 'Invalid JSON payload');
        this.lastHandshakeFailure = { reason: 'invalid-json', at: new Date() };
        this.emitAutomation('handshakeFailed', { reason: 'invalid-json', port: this.clientPort });
        return;
      }

      if (!handshakeComplete) {
        if (parsed.type !== 'bridge_ack') {
          log.warn(`Expected bridge_ack handshake, received ${parsed.type}; closing.`);
          socket.close(4004, 'Handshake expected bridge_ack');
          this.lastHandshakeFailure = { reason: 'invalid-handshake', at: new Date() };
          this.emitAutomation('handshakeFailed', { reason: 'invalid-handshake', port: this.clientPort });
          return;
        }

        handshakeComplete = true;
        clearTimeout(handshakeTimeout);
        const now = new Date();
        const metadata = this.sanitizeHandshakeMetadata(parsed as Record<string, unknown>);
        this.lastHandshakeMetadata = metadata;
        try {
          log.info(`Automation bridge client handshake ack received (port=${this.clientPort}) metadata=${JSON.stringify(metadata)}`);
        } catch (_e) {
          log.info('Automation bridge client handshake ack received (port=%d) [metadata redaction failed]', this.clientPort);
        }
        this.lastHandshakeAck = parsed;
        this.lastHandshakeFailure = undefined;
        this.lastMessageAt = now;
        // Attempt to extract remote address/port from the underlying socket
        const underlying: any = (socket as any)._socket || (socket as any).socket;
        const remoteAddr = underlying?.remoteAddress ?? undefined;
        const remotePort = underlying?.remotePort ?? undefined;
        this.registerSocket(socket, this.clientPort, metadata, remoteAddr, remotePort);
        this.startHeartbeat();
        this.emitAutomation('connected', {
          socket,
          metadata,
          port: this.clientPort,
          protocol: socket.protocol || null
        });
        return;
      }

      this.handleAutomationMessage(parsed);
      this.lastMessageAt = new Date();
      this.emitAutomation('message', parsed);
    });

    socket.on('error', (error) => {
      log.error('Automation bridge client socket error', error);
      const errObj = error instanceof Error ? error : new Error(String(error));
      this.lastError = { message: errObj.message, at: new Date() };
      const errWithPort = Object.assign(errObj, { port: this.clientPort });
      this.emitAutomation('error', errWithPort);
    });

    socket.on('close', (code, reasonBuffer) => {
      const reason = reasonBuffer.toString('utf8');
      const socketInfo = this.activeSockets.get(socket);
      if (socketInfo) {
        this.activeSockets.delete(socket);
        // If this was the primary socket, choose a new primary
        if (socket === this.primarySocket) {
          this.primarySocket = this.activeSockets.size > 0 ? this.activeSockets.keys().next().value : undefined;
          if (this.activeSockets.size === 0) {
            this.stopHeartbeat();
          }
        }
        clearTimeout(handshakeTimeout);
        this.lastDisconnect = { code, reason, at: new Date() };
        this.emitAutomation('disconnected', {
          code,
          reason,
          port: socketInfo.port,
          protocol: socketInfo.protocol || null
        });
        log.info(`Automation bridge client socket closed (code=${code}, reason=${reason})`);
        // Only reject pending requests if this was the last connection
        if (this.activeSockets.size === 0) {
          this.rejectAllPending(new Error(`Automation bridge connection closed (${code}): ${reason || 'no reason'}`));
        }
      }
    });

    // Handle WebSocket pong frames for heartbeat tracking
    socket.on('pong', () => {
      this.lastMessageAt = new Date();
    });
  }

  private registerSocket(
    socket: WebSocket,
    port: number,
    metadata?: Record<string, unknown>,
    remoteAddress?: string,
    remotePort?: number
  ) {
    const connectionId = randomUUID();
    const sessionId = metadata && typeof metadata.sessionId === 'string' ? (metadata.sessionId as string) : undefined;
    const socketInfo = {
      connectionId,
      port,
      connectedAt: new Date(),
      protocol: socket.protocol || undefined,
      sessionId,
      remoteAddress: remoteAddress ?? undefined,
      remotePort: typeof remotePort === 'number' ? remotePort : undefined
    };

    this.activeSockets.set(socket, socketInfo);

    // Set as primary socket if this is the first connection
    if (!this.primarySocket) {
      this.primarySocket = socket;
      this.connectedAt = socketInfo.connectedAt;
    }

    // Handle WebSocket pong frames for heartbeat tracking
    socket.on('pong', () => {
      this.lastMessageAt = new Date();
    });
  }

  private sanitizeHandshakeMetadata(payload: Record<string, unknown>): Record<string, unknown> {
    const sanitized: Record<string, unknown> = { ...payload };
    delete sanitized.type;
    if ('capabilityToken' in sanitized) {
      sanitized.capabilityToken = 'REDACTED';
    }
    return sanitized;
  }

  private createHandshakeAck(handshake: Record<string, unknown>, port: number, socket: WebSocket): AutomationBridgeMessage {
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
      activePort: port,
      protocol: socket.protocol || null,
      // activeSockets already includes the connecting socket by the time we
      // build the handshake ack, so use the map size directly.
      concurrentConnections: this.activeSockets.size,
      maxConcurrentConnections: this.maxConcurrentConnections
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
      case 'bridge_pong':
        // Heartbeat acknowledgements require no action beyond updating timestamps.
        break;
      case 'bridge_goodbye':
        log.info('Automation bridge client initiated shutdown.', message);
        break;
      case 'automation_event': {
        // Some plugin handlers may emit an 'automation_event' to signal
        // completion of a long-running request (for example, modify_scs
        // may schedule a deferred apply and later notify completion).
        // If the event includes a requestId that matches a pending
        // automation request, resolve it as a success using the
        // provided result payload.
        const evt: any = message as any;
        const reqId = typeof evt.requestId === 'string' ? evt.requestId : undefined;
        if (reqId) {
          const pending = this.pendingRequests.get(reqId);
          if (pending) {
            try {
              // Clear both the initial response timeout and any event timeout
              clearTimeout(pending.timeout);
              if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
              this.pendingRequests.delete(reqId);
              const baseSuccess = (pending.initialResponse && typeof pending.initialResponse.success === 'boolean') ? pending.initialResponse.success : undefined;
              const evtSuccess = (evt.result && typeof evt.result.success === 'boolean') ? !!evt.result.success : undefined;
              const synthetic: AutomationBridgeResponseMessage = {
                type: 'automation_response',
                requestId: reqId,
                success: evtSuccess !== undefined ? evtSuccess : (baseSuccess !== undefined ? baseSuccess : undefined as any),
                message: typeof evt.result?.message === 'string' ? evt.result.message : (typeof evt.message === 'string' ? evt.message : FStringSafe(evt.event)),
                error: typeof evt.result?.error === 'string' ? evt.result.error : undefined,
                result: evt.result ?? evt.payload ?? undefined
              };
              log.info(`automation_event resolved pending request ${reqId} (event=${String(evt.event || '')})`);
              pending.resolve(synthetic);
            } catch (e) {
              log.warn(`Failed to resolve pending automation request from automation_event ${reqId}: ${String(e)}`);
            }
            return;
          }
        }
        // No pending request matched; treat as a generic event.
        log.debug('Received automation_event (no pending request):', message);
        break;
      }
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
      log.debug(`No pending automation request found for requestId=${requestId}; pendingRequests=${this.pendingRequests.size}. Response payload truncated: ${JSON.stringify(response).substring(0, 1000)}`);
      return;
    }

    // Helper: enforce that any action echoed by the plugin matches the requested action (prefix-safe)
    const enforceActionMatch = (resp: AutomationBridgeResponseMessage): AutomationBridgeResponseMessage => {
      try {
        const expected = (pending.action || '').toString().toLowerCase();
        const echoed: string | undefined = (() => {
          const r: any = resp as any;
          const candidate = (typeof r.action === 'string' && r.action) || (typeof r.result?.action === 'string' && r.result.action);
          return candidate as any;
        })();
        if (expected && echoed && typeof echoed === 'string') {
          const got = echoed.toLowerCase();
          // Consider it a mismatch if neither starts with the other (allowing minor decoration, e.g. namespaces)
          const startsEitherWay = got.startsWith(expected) || expected.startsWith(got);
          if (!startsEitherWay) {
            const mutated: any = { ...resp };
            // Mark as failure without discarding original payload
            mutated.success = false;
            if (!mutated.error) mutated.error = 'ACTION_PREFIX_MISMATCH';
            const msgBase = typeof mutated.message === 'string' ? mutated.message + ' ' : '';
            mutated.message = `${msgBase}Response action mismatch (expected~='${expected}', got='${echoed}')`;
            return mutated as AutomationBridgeResponseMessage;
          }
        }
      } catch (e) {
        log.debug('enforceActionMatch check skipped', e as any);
      }
      return response;
    };

    // If the caller requested waiting for a completion event, defer
    // resolution until the automation_event matching this requestId
    // arrives. However, if the response already indicates a final on-disk
    // save (result.saved===true) or a definitive failure, resolve now.
    if (pending.waitForEvent) {
      // If we have not yet seen an initial response for this request,
      // treat this response as the initial acknowledgement and store it
      // while we await the completion event. If we have already stored
      // an initial response and we receive another automation_response,
      // treat the latter as the final completion response and resolve.
      if (!pending.initialResponse) {
        // Clear the initial response timeout — we received a response.
        clearTimeout(pending.timeout);
        const r: any = response.result ?? response;
        const savedFlag = r && typeof r.saved !== 'undefined' ? r.saved : undefined;
        // If the plugin reports a final persisted state or a failure,
        // resolve immediately instead of waiting for an event.
        if (response.success === false || savedFlag === true) {
          this.pendingRequests.delete(requestId);
          pending.resolve(enforceActionMatch(response));
          return;
        }
        // Store initial response and wait for an automation_event or
        // a subsequent automation_response to mark completion.
        pending.initialResponse = enforceActionMatch(response);
        const evtMs = pending.eventTimeoutMs && pending.eventTimeoutMs > 0 ? pending.eventTimeoutMs : undefined;
        if (evtMs !== undefined) {
          pending.eventTimeout = setTimeout(() => {
            if (this.pendingRequests.has(requestId)) this.pendingRequests.delete(requestId);
            try {
              pending.reject(new Error(`Timed out waiting for completion event for request ${requestId} after ${evtMs}ms`));
            } catch {}
          }, evtMs);
        } else {
          log.debug(`Waiting indefinitely for completion event for request ${requestId}; no event timeout configured.`);
        }
        // Leave the pending entry in the map so automation_event or
        // another automation_response can complete it.
        return;
      } else {
        // We previously stored an initial response and now received a
        // second automation_response; treat this as the final result.
        if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
        this.pendingRequests.delete(requestId);
        pending.resolve(enforceActionMatch(response));
        return;
      }
    }

    clearTimeout(pending.timeout);
    this.pendingRequests.delete(requestId);
    pending.resolve(enforceActionMatch(response));
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
      if (!this.isConnected() || !this.primarySocket) {
        this.stopHeartbeat();
        return;
      }

      // Use WebSocket ping control frame instead of JSON data frame to avoid fragmentation
      try {
        this.primarySocket.ping();
      } catch (error) {
        log.warn('Failed to send heartbeat ping', error);
        this.stopHeartbeat();
      }
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
    options: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number } = {}
  ): Promise<AutomationBridgeResponseMessage> {
    // Default timeout for automation requests. Allow override via
    // MCP_AUTOMATION_REQUEST_TIMEOUT_MS (ms). Default is 120s but for
    // blueprint-related or Python-executing actions we enforce at least
    // 120s to avoid spurious 60s timeouts on large editor ops.
    const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
    let defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
    // Ensure blueprint and Python-heavy actions have a minimum timeout
    const lowerAction = (action || '').toLowerCase();
    if (lowerAction.includes('blueprint') || lowerAction.includes('execute_editor_python') || lowerAction.includes('modify_scs')) {
      // Blueprint & Python-heavy operations can be long; prefer a larger
      // default timeout to accommodate large editor ops (compile/save, I/O).
      defaultTimeout = Math.max(defaultTimeout, 300000); // 5 minutes
    }
    const timeoutMs = Math.max(1000, options.timeoutMs ?? defaultTimeout);

    // Decide whether to wait for a completion signal for this action.
    // If the caller specified waitForEvent explicitly, honor it; otherwise
    // enable it for known long-running/editor-mutating actions to avoid
    // reporting success on initial acknowledgements. Certain actions are
    // explicitly known to return a single, final automation_response and
    // must never wait for an event.
    const normalizedAction = String(action || '').toLowerCase();
    let useWaitForEvent = typeof options.waitForEvent === 'boolean'
      ? options.waitForEvent
      : WAIT_FOR_EVENT_ACTIONS.has(normalizedAction);
    // Force-disable event waiting for actions that are known to complete
    // with a single response (no automation_event follow-up).
    const ACTIONS_NO_EVENT = new Set<string>([
      'delete_assets', 'rename_asset', 'duplicate_asset', 'move_asset', 'create_thumbnail', 'set_tags', 'validate', 'generate_report',
      'import_asset', 'import_asset_deferred', 'create_material', 'create_material_instance', 'asset_exists',
      'execute_console_command', 'control_actor', 'control_editor', 'stream_level', 'save_current_level'
    ]);
    if (ACTIONS_NO_EVENT.has(normalizedAction)) {
      useWaitForEvent = false;
    }
    const envEventTimeoutRaw = process.env.MCP_AUTOMATION_EVENT_TIMEOUT_MS;
    const envEventTimeout = envEventTimeoutRaw ? Number(envEventTimeoutRaw) : undefined;
    const defaultEventTimeout = envEventTimeout !== undefined && Number.isFinite(envEventTimeout) && envEventTimeout > 0
      ? envEventTimeout
      : undefined;
    const waitEventTimeoutMs = typeof options.waitForEventTimeoutMs === 'number' && options.waitForEventTimeoutMs > 0
      ? options.waitForEventTimeoutMs
      : defaultEventTimeout;

    // Helper: stable stringify to produce deterministic cache keys for payloads
    const stableStringify = (value: unknown): string => {
      const replacer = (_key: string, val: any) => {
        if (val && typeof val === 'object' && !Array.isArray(val)) {
          const sorted: any = {};
          Object.keys(val).sort().forEach((k) => { sorted[k] = val[k]; });
          return sorted;
        }
        return val;
      };
      try { return JSON.stringify(value, replacer); } catch { return String(value); }
    };

    // Coalesce identical frequent read-only requests to avoid spamming the editor
    const COALESCE_ACTIONS = new Set(['blueprint_exists']);
    let coalesceKey: string | undefined;
    if (COALESCE_ACTIONS.has(action)) {
      try {
        coalesceKey = `${action}:${stableStringify(payload)}`;
        const existing = this.coalescedRequests.get(coalesceKey);
        if (existing) {
          log.debug(`Coalescing automation request for action=${action}`);
          return existing;
        }
      } catch (_err) {
        // ignore failures to stringify payloads when building a coalesce key
        coalesceKey = undefined;
      }
    }

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
        requestedAt: new Date(),
        waitForEvent: useWaitForEvent,
        eventTimeoutMs: waitEventTimeoutMs
      });
    });

    this.lastRequestSentAt = new Date();
    // Avoid noisy logs for very frequent probe actions
    const FREQUENT_ACTIONS = new Set(['blueprint_exists']);
    if (FREQUENT_ACTIONS.has(action)) {
      log.debug(`sendAutomationRequest: requestId=${requestId}, action=${action}, timeoutMs=${timeoutMs}${useWaitForEvent ? ', waitForEvent=true' : ''}`);
    } else {
      log.info(`sendAutomationRequest: requestId=${requestId}, action=${action}, timeoutMs=${timeoutMs}${useWaitForEvent ? ', waitForEvent=true' : ''}`);
    }
    const sent = this.send(requestPayload);
    if (!sent) {
      const pending = this.pendingRequests.get(requestId);
      if (pending) {
        clearTimeout(pending.timeout);
        this.pendingRequests.delete(requestId);
      }
      throw new Error('Failed to dispatch automation request');
    }

    // Register coalesced promise so other callers with the same action+payload
    // will reuse the same in-flight request instead of sending duplicates.
    if (coalesceKey) {
      this.coalescedRequests.set(coalesceKey, pendingPromise);
      // Ensure we remove the coalesced entry when the promise settles
      const localKey = coalesceKey;
      pendingPromise.finally(() => { if (localKey) { this.coalescedRequests.delete(localKey); } }).catch(() => {});
    }

    return pendingPromise;
  }

  async controlEnvironment(
    action: string,
    payload: Record<string, unknown> = {},
    options: { timeoutMs?: number } = {}
  ): Promise<AutomationBridgeResponseMessage> {
    return this.sendAutomationRequest('control_environment', { action, ...payload }, options);
  }

  private emitAutomation<K extends keyof AutomationBridgeEvents>(
    event: K,
    payload: Parameters<AutomationBridgeEvents[K]>[0]
  ): void {
    this.emit(event, payload);
  }
}

export type AutomationBridgeEvent = keyof AutomationBridgeEvents;