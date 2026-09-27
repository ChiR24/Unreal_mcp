import net from 'node:net';
import {
    DEFAULT_AUTOMATION_HOST,
    DEFAULT_AUTOMATION_PORT,
    DEFAULT_HEARTBEAT_INTERVAL_MS,
    DEFAULT_MAX_INBOUND_MESSAGES_PER_MINUTE,
    DEFAULT_MAX_PENDING_REQUESTS,
    DEFAULT_MAX_QUEUED_REQUESTS,
    DEFAULT_NEGOTIATED_PROTOCOLS,
    PACKAGE
} from '../constants.js';
import { config } from '../config.js';
import { readProjectIniValue } from '../utils/config/ini-reader.js';
import type { Logger } from '../utils/logging/logger.js';
import type { AutomationBridgeOptions } from './types.js';

type BridgeConfigLogger = Pick<Logger, 'debug' | 'warn' | 'error'>;

const BRIDGE_SETTINGS_SECTION = '/Script/McpAutomationBridge.McpAutomationBridgeSettings';
const BRIDGE_SETTINGS_CATEGORY = 'Game';

/**
 * First `ListenPorts` token from the project's own config, for projects that
 * do not pin `MCP_AUTOMATION_PORT`. The plugin binds every configured token in
 * order and a busy port silently drops out of the set, so the first token is
 * the one a client should dial. Best-effort and read-only: a missing project,
 * file, section or key keeps the built-in default.
 */
function readProjectListenPort(log: BridgeConfigLogger): number | null {
    const projectPath = process.env.UE_PROJECT_PATH;
    if (!projectPath) {
        return null;
    }

    try {
        const raw = readProjectIniValue(projectPath, BRIDGE_SETTINGS_CATEGORY, BRIDGE_SETTINGS_SECTION, 'ListenPorts');
        if (raw === undefined) {
            return null;
        }

        const port = sanitizePort(raw.split(',')[0]?.trim());
        if (port === null) {
            return null;
        }

        log.debug(`Resolved automation bridge port ${port} from ${projectPath} ListenPorts.`);
        return port;
    } catch {
        return null;
    }
}

export interface AutomationBridgeResolvedConfig {
    /** The one endpoint the client dials. */
    readonly host: string;
    readonly port: number;
    readonly negotiatedProtocols: string[];
    readonly capabilityToken?: string;
    readonly enabled: boolean;
    readonly serverName: string;
    readonly serverVersion: string;
    readonly maxQueuedRequests: number;
    readonly maxPendingRequests: number;
    readonly useTls: boolean;
    readonly connectionTimeoutMs: number;
    readonly heartbeatIntervalMs: number;
    readonly maxInboundMessagesPerMinute: number;
}

export type BridgeFailureReason =
    | 'connection refused'
    | 'timed out'
    | 'host unreachable'
    | 'tls failure'
    | 'handshake rejected'
    | 'connection lost'
    | 'server stopped'
    | 'bridge disabled'
    | 'unknown failure';

/**
 * Map a raw connection exception onto a closed-set reason. Tool output must not
 * carry free-form OS, TLS or peer text - the peer controls parts of the
 * handshake strings (for example the received message type) - so callers put the
 * mapped reason in the user-facing message and keep the full exception in the
 * trusted logger.
 */
// Order matters: our own protocol markers win over generic words a peer can
// embed in the received handshake string (a `type` value of `timeout`, say).
const FAILURE_PATTERNS: readonly (readonly [RegExp, BridgeFailureReason])[] = [
    [/BRIDGE_ACK/, 'handshake rejected'],
    [/ECONNREFUSED/, 'connection refused'],
    // A refused WebSocket upgrade (401/426 and friends) is a handshake reject.
    [/UNEXPECTED SERVER RESPONSE|INCORRECT STATUS CODE/, 'handshake rejected'],
    [/SERVER STOPPED/, 'server stopped'],
    [/\bDISABLED\b/, 'bridge disabled'],
    [/ETIMEDOUT|UND_ERR_CONNECT_TIMEOUT|TIMEOUT/, 'timed out'],
    [/ENOTFOUND|EAI_AGAIN|EHOSTUNREACH|ENETUNREACH|EADDRNOTAVAIL/, 'host unreachable'],
    [/ERR_TLS|TLS|SSL|CERT|SELF_SIGNED/, 'tls failure'],
    [/ECONNRESET|EPIPE|SOCKET HANG UP|SOCKET CLOSED/, 'connection lost'],
    [/HANDSHAKE|INVALID_CAPABILITY_TOKEN|CAPABILITY TOKEN/, 'handshake rejected'],
];

export function describeBridgeFailure(cause: unknown): BridgeFailureReason {
    const code = typeof cause === 'object' && cause !== null && 'code' in cause
        ? String((cause as { code?: unknown }).code ?? '')
        : '';
    const message = cause instanceof Error ? cause.message : String(cause ?? '');
    const match = (text: string) => FAILURE_PATTERNS.find(([pattern]) => pattern.test(text))?.[1];
    // Structured transport codes are trustworthy; message text is not (peer
    // handshake strings land in it), so the code decides first.
    return (code ? match(code.toUpperCase()) : undefined) ?? match(`${code} ${message}`.toUpperCase()) ?? 'unknown failure';
}

/**
 * One wording for every "bridge is not there" failure so logs, tool output and
 * telemetry agree. Callers pass the resolved target and, when available, a
 * closed-set reason from {@link describeBridgeFailure}. Always includes
 * `not connected` - transport classification in `services/telemetry-observation.ts`
 * matches that marker.
 */
export function bridgeNotConnectedMessage(target?: string, reason?: BridgeFailureReason): string {
    const where = target ? ` at ${target}` : '';
    const why = reason ? `: ${reason}` : '';
    return `Automation bridge not connected${where}${why}. Ensure the Unreal Editor is running with the automation bridge listening.`;
}

export function formatHostForUrl(host: string): string {
    if (!host.includes(':')) {
        return host;
    }

    const zoneIndex = host.indexOf('%');
    const hostWithoutZone = zoneIndex >= 0 ? host.slice(0, zoneIndex) : host;
    return `[${hostWithoutZone}]`;
}

export function resolveAutomationBridgeConfig(
    options: AutomationBridgeOptions,
    log: BridgeConfigLogger
): AutomationBridgeResolvedConfig {
    const allowNonLoopback = options.allowNonLoopback
        ?? (process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK?.toLowerCase() === 'true');

    const host = normalizeHost(
        options.host
            ?? process.env.MCP_AUTOMATION_CLIENT_HOST
            ?? process.env.MCP_AUTOMATION_WS_HOST
            ?? process.env.MCP_AUTOMATION_HOST
            ?? DEFAULT_AUTOMATION_HOST,
        'Automation bridge host', allowNonLoopback, log);
    // Explicit options or environment win; the project's own ListenPorts is the
    // fallback so a per-project entry needs nothing but UE_PROJECT_PATH.
    const port = sanitizePort(options.port)
        ?? sanitizePort(process.env.MCP_AUTOMATION_CLIENT_PORT)
        ?? sanitizePort(process.env.MCP_AUTOMATION_WS_PORT)
        ?? sanitizePort(process.env.MCP_AUTOMATION_PORT)
        ?? readProjectListenPort(log)
        ?? DEFAULT_AUTOMATION_PORT;
    const requestedHeartbeatMs = options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS;
    const heartbeatIntervalMs = requestedHeartbeatMs > 0 ? requestedHeartbeatMs : 0;

    return {
        host,
        port,
        negotiatedProtocols: resolveProtocols(options.protocols),
        capabilityToken: options.capabilityToken ?? process.env.MCP_AUTOMATION_CAPABILITY_TOKEN ?? undefined,
        enabled: options.enabled ?? process.env.MCP_AUTOMATION_BRIDGE_ENABLED !== 'false',
        serverName: options.serverName ?? process.env.MCP_SERVER_NAME ?? PACKAGE.name,
        serverVersion: options.serverVersion ?? process.env.MCP_SERVER_VERSION ?? PACKAGE.version,
        maxQueuedRequests: Math.max(0, options.maxQueuedRequests ?? DEFAULT_MAX_QUEUED_REQUESTS),
        maxPendingRequests: Math.max(1, options.maxPendingRequests ?? DEFAULT_MAX_PENDING_REQUESTS),
        useTls: parseBoolean(options.useTls ?? process.env.MCP_AUTOMATION_USE_TLS, false),
        connectionTimeoutMs: Math.max(
            1,
            parseNonNegativeInt(options.connectionTimeoutMs ?? config.MCP_CONNECTION_TIMEOUT_MS, config.MCP_CONNECTION_TIMEOUT_MS)
        ),
        heartbeatIntervalMs,
        maxInboundMessagesPerMinute: parseNonNegativeInt(
            options.maxInboundMessagesPerMinute ?? process.env.MCP_AUTOMATION_MAX_MESSAGES_PER_MINUTE,
            DEFAULT_MAX_INBOUND_MESSAGES_PER_MINUTE
        )
    };
}

function resolveProtocols(optionProtocols: string[] | undefined): string[] {
    const userProtocols = Array.isArray(optionProtocols)
        ? optionProtocols.filter((proto) => typeof proto === 'string' && proto.trim().length > 0)
        : [];
    const envProtocols = process.env.MCP_AUTOMATION_WS_PROTOCOLS
        ? process.env.MCP_AUTOMATION_WS_PROTOCOLS.split(',')
            .map((token) => token.trim())
            .filter((token) => token.length > 0)
        : [];

    return Array.from(new Set([...userProtocols, ...envProtocols, ...DEFAULT_NEGOTIATED_PROTOCOLS]));
}

function normalizeHost(value: unknown, label: string, allowNonLoopback: boolean, log: BridgeConfigLogger): string {
    const stringValue = typeof value === 'string' ? value : value === undefined || value === null ? '' : String(value);
    const trimmed = stringValue.trim();
    if (trimmed.length === 0) {
        return DEFAULT_AUTOMATION_HOST;
    }

    const lower = trimmed.toLowerCase();
    if (lower === 'localhost' || lower === '127.0.0.1') return '127.0.0.1';
    if (lower === '::1' || lower === '[::1]') return '::1';

    if (allowNonLoopback) {
        const normalizedAddress = trimIpv6Brackets(trimmed);
        const addressWithoutZone = normalizedAddress.split('%')[0] ?? normalizedAddress;
        const ipVersion = net.isIP(addressWithoutZone);
        if (ipVersion === 4 || ipVersion === 6) {
            log.warn(`SECURITY: ${label} set to non-loopback address '${trimmed}'. The automation bridge will be accessible from your local network.`);
            return normalizedAddress;
        }
        if (isValidHostname(trimmed)) {
            log.warn(`SECURITY: ${label} set to hostname '${trimmed}'. The automation bridge will be accessible from your local network.`);
            return trimmed;
        }

        log.error(`${label} '${trimmed}' is not a valid IPv4/IPv6 address or hostname. Falling back to ${DEFAULT_AUTOMATION_HOST}.`);
        return DEFAULT_AUTOMATION_HOST;
    }

    log.warn(`${label} '${trimmed}' is not a loopback address and MCP_AUTOMATION_ALLOW_NON_LOOPBACK is not set. Falling back to ${DEFAULT_AUTOMATION_HOST}. Set MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true for LAN access.`);
    return DEFAULT_AUTOMATION_HOST;
}

function trimIpv6Brackets(value: string): string {
    return value.startsWith('[') && value.endsWith(']') ? value.slice(1, -1) : value;
}

function isValidHostname(value: string): boolean {
    if (!/[a-zA-Z]/.test(value)) {
        return false;
    }

    return value
        .split('.')
        .every((label) => label.length > 0 && /^[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?$/.test(label));
}

function sanitizePort(value: unknown): number | null {
    if (typeof value === 'number' && Number.isInteger(value)) {
        return value > 0 && value <= 65535 ? value : null;
    }
    if (typeof value === 'string' && value.trim().length > 0) {
        const trimmed = value.trim();
        if (!/^\d+$/.test(trimmed)) return null;
        const parsed = Number(trimmed);
        return Number.isInteger(parsed) && parsed > 0 && parsed <= 65535 ? parsed : null;
    }
    return null;
}

function parseNonNegativeInt(value: unknown, fallback: number): number {
    if (typeof value === 'number' && Number.isInteger(value)) {
        return value >= 0 ? value : fallback;
    }
    if (typeof value === 'string' && value.trim().length > 0) {
        const trimmed = value.trim();
        if (!/^\d+$/.test(trimmed)) return fallback;
        const parsed = Number(trimmed);
        return Number.isInteger(parsed) && parsed >= 0 ? parsed : fallback;
    }
    return fallback;
}

function parseBoolean(value: unknown, defaultValue: boolean): boolean {
    if (typeof value === 'boolean') return value;
    if (typeof value === 'string') {
        const normalized = value.trim().toLowerCase();
        if (normalized === 'true') return true;
        if (normalized === 'false') return false;
    }
    return defaultValue;
}

