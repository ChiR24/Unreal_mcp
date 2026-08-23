import { WebSocket } from 'ws';
import type { LiveStateRevisions } from '../tools/catalog/capabilities/semantic/live-state-revisions.js';

export interface AutomationBridgeOptions {
    host?: string | null;
    port?: number;
    ports?: number[];
    protocols?: string[];
    capabilityToken?: string;
    enabled?: boolean;
    serverName?: string;
    serverVersion?: string;
    heartbeatIntervalMs?: number;
    connectionTimeoutMs?: number;
    maxPendingRequests?: number;
    maxConcurrentConnections?: number;
    maxQueuedRequests?: number;
    maxInboundMessagesPerMinute?: number;
    maxInboundAutomationRequestsPerMinute?: number;
    useTls?: boolean;
    clientMode?: boolean;
    clientHost?: string;
    clientPort?: number;
    serverLegacyEnabled?: boolean;
    /** SECURITY: Allow non-loopback host binding for LAN access. Default: false (loopback-only). */
    allowNonLoopback?: boolean;
}

export interface AutomationBridgeMessage {
    type: string;
    [key: string]: unknown;
}

export interface AutomationBridgeAutomationEvent {
    [key: string]: unknown;
    type: 'automation_event';
    event: string;
    requestId?: string;
    payload?: unknown;
    result?: unknown;
    message?: string;
}

/**
 * Targeted cancellation frame sent from the TS bridge to Unreal when an MCP
 * request is cancelled. Carries an already-generated automation request id so
 * the plugin can cancel the exact correlated operation.
 */
export interface CancelRequestMessage extends AutomationBridgeMessage {
    type: 'cancel_request';
    /** The automation request id previously allocated by the bridge. */
    requestId: string;
    /** Optional human-readable cancellation reason. */
    reason?: string;
}

export interface AutomationBridgeResponseMessage extends AutomationBridgeMessage {
    requestId: string;
    success?: boolean;
    message?: string;
    error?: string;
    result?: unknown;
    liveRevisions?: LiveStateRevisions;
}

export type PendingRequestDetail = { requestId: string; action: string; ageMs: number };
export type AutomationBridgeConnectionInfo = {
    connectionId: string;
    sessionId: string | null;
    remoteAddress: string | null;
    remotePort: number | null;
    port: number;
    connectedAt: string;
    protocol: string | null;
    readyState: number;
    isPrimary: boolean;
};
export type AutomationBridgeConnectedEvent = { socket: WebSocket; metadata: Record<string, unknown>; port: number; protocol: string | null };
export type AutomationBridgeDisconnectedEvent = { code: number; reason: string; port: number; protocol: string | null };
export type AutomationBridgePortError = Error & { port?: number };

/**
 * Progress update message sent by UE during long operations.
 * Used to extend request timeout and provide status feedback.
 */
export interface ProgressUpdateMessage extends AutomationBridgeMessage {
    type: 'progress_update';
    requestId: string;
    percent?: number;       // 0-100 progress indicator
    message?: string;       // Human-readable status
    timestamp?: string;     // ISO timestamp
    stillWorking?: boolean; // True if operation is still in progress
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
    pendingRequestDetails: PendingRequestDetail[];
    connections: AutomationBridgeConnectionInfo[];
    webSocketListening: boolean;
    serverLegacyEnabled: boolean;
    serverName: string;
    serverVersion: string;
    maxConcurrentConnections: number;
    maxPendingRequests: number;
    heartbeatIntervalMs: number;
}

/**
 * One progress observation forwarded from Unreal toward the MCP client.
 *
 * Structurally identical to the server-side progress primitive's update shape,
 * declared here so the automation layer never has to import upward from the
 * server layer to describe its own outbound signal.
 */
export interface AutomationProgressUpdate {
    readonly progress: number;
    readonly total?: number;
    readonly message?: string;
}

/** Receives progress already resolved to the MCP request that owns it. */
export type AutomationProgressListener = (
    mcpRequestId: string,
    update: AutomationProgressUpdate
) => void;

/**
 * Object-style specification for a tracked automation request. Kept as one
 * object so future concerns (e.g. ownership for Todo 7) can extend it without
 * another positional signature change.
 */
export interface RequestTrackerRequestSpec {
    readonly action: string;
    readonly payload: Record<string, unknown>;
    readonly timeoutMs: number;
}

/**
 * The natural tracker timeout classes that terminate a request without an
 * external response. Each maps to exactly one best-effort advisory
 * `cancel_request` frame emitted by the dispatcher.
 */
export type NaturalTimeoutKind =
    | 'ordinary_deadline'
    | 'progress_extension_deadline'
    | 'stale_progress'
    | 'extension_cap'
    | 'absolute_deadline';

/** Typed terminal notification for a request the tracker settled by natural timeout. */
export interface NaturalTimeoutNotification {
    readonly requestId: string;
    readonly action: string;
    readonly kind: NaturalTimeoutKind;
    readonly error: Error;
}

/** Receives natural-timeout terminal notifications. */
export type NaturalTimeoutObserver = (notification: NaturalTimeoutNotification) => void;

export interface PendingRequest {
    resolve: (value: AutomationBridgeResponseMessage) => void;
    reject: (reason: Error) => void;
    timeout: NodeJS.Timeout;
    action: string;
    payload: Record<string, unknown>;
    requestedAt: Date;
    waitForEvent?: boolean;
    eventTimeout?: NodeJS.Timeout | undefined;
    eventTimeoutMs?: number | undefined;
    initialResponse?: AutomationBridgeResponseMessage | undefined;
    // Progress tracking for timeout extension
    extensionCount?: number;
    lastProgressPercent?: number;
    staleCount?: number;
    absoluteTimeout?: NodeJS.Timeout;
    totalExtensionMs?: number;
    /** Connection id of the socket that carried this request's frame (Todo 7 ownership). */
    ownerId?: string;
}

/**
 * Represents a queued request item waiting to be sent when capacity is available.
 * Uses unknown for resolve/reject values since the queue stores items from different
 * generic Promise<T> contexts.
 */
export interface QueuedRequestItem {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    action: string;
    payload: Record<string, unknown>;
    options: Record<string, unknown>;
    /** Canonicalized MCP request id that owns this queued item, if any. */
    mcpRequestId?: string;
}

export interface SocketInfo {
    connectionId: string;
    port: number;
    connectedAt: Date;
    protocol?: string;
    sessionId?: string;
    remoteAddress?: string;
    remotePort?: number;
}

export type AutomationBridgeEvents = {
    connected: (info: AutomationBridgeConnectedEvent) => void;
    disconnected: (info: AutomationBridgeDisconnectedEvent) => void;
    message: (message: AutomationBridgeMessage) => void;
    automationEvent: (event: AutomationBridgeAutomationEvent) => void;
    error: (error: AutomationBridgePortError) => void;
    handshakeFailed: (info: { reason: string; port: number }) => void;
};
