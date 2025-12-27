import { WebSocket } from 'ws';

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
    maxQueuedRequests?: number;
    clientMode?: boolean;
    clientHost?: string;
    clientPort?: number;
    serverLegacyEnabled?: boolean;
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
    connected: (info: { socket: WebSocket; metadata: Record<string, unknown>; port: number; protocol: string | null }) => void;
    disconnected: (info: { code: number; reason: string; port: number; protocol: string | null }) => void;
    message: (message: AutomationBridgeMessage) => void;
    error: (error: Error & { port?: number }) => void;
    handshakeFailed: (info: { reason: string; port: number }) => void;
};
