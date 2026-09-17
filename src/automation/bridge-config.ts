import { createRequire } from 'node:module';
import net from 'node:net';
import {
    DEFAULT_AUTOMATION_HOST,
    DEFAULT_AUTOMATION_PORT,
    DEFAULT_HEARTBEAT_INTERVAL_MS,
    DEFAULT_MAX_INBOUND_AUTOMATION_REQUESTS_PER_MINUTE,
    DEFAULT_MAX_INBOUND_MESSAGES_PER_MINUTE,
    DEFAULT_MAX_PENDING_REQUESTS,
    DEFAULT_MAX_QUEUED_REQUESTS,
    DEFAULT_NEGOTIATED_PROTOCOLS
} from '../constants.js';
import { config } from '../config.js';
import { getProjectSettingSync } from '../utils/config/ini-reader.js';
import type { Logger } from '../utils/logging/logger.js';
import type { AutomationBridgeOptions } from './types.js';

const requirePackage = createRequire(import.meta.url);

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
        const raw = getProjectSettingSync(projectPath, BRIDGE_SETTINGS_CATEGORY, BRIDGE_SETTINGS_SECTION, 'ListenPorts');
        if (typeof raw !== 'string') {
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

interface PackageInfo {
    readonly name?: string;
    readonly version?: string;
}

export interface AutomationBridgeResolvedConfig {
    readonly host: string;
    readonly port: number;
    readonly ports: number[];
    readonly negotiatedProtocols: string[];
    readonly capabilityToken?: string;
    readonly enabled: boolean;
    readonly serverName: string;
    readonly serverVersion: string;
    readonly clientHost: string;
    readonly clientPort: number;
    readonly serverLegacyEnabled: boolean;
    readonly maxConcurrentConnections: number;
    readonly maxQueuedRequests: number;
    readonly maxPendingRequests: number;
    readonly useTls: boolean;
    readonly connectionTimeoutMs: number;
    readonly heartbeatIntervalMs: number;
    readonly maxInboundMessagesPerMinute: number;
    readonly maxInboundAutomationRequestsPerMinute: number;
}

/**
 * Human-readable `host:ports` target list for diagnostics. The host is
 * bracketed for IPv6 so the result stays unambiguous when several ports are
 * reported at once (`::1:8090,8091` would otherwise read as a single colon).
 */
export function formatBridgeTarget(host: string, ports: readonly number[]): string {
    return `${formatHostForUrl(host)}:${ports.join(',')}`;
}

/**
 * One wording for every "bridge is not there" failure so logs, tool output and
 * telemetry agree. Callers pass the resolved target and, when available, the
 * underlying cause. Always includes `not connected` - transport classification
 * in `services/telemetry-observation.ts` matches that marker.
 */
export function bridgeNotConnectedMessage(target?: string, cause?: string): string {
    const where = target ? ` at ${target}` : '';
    const why = cause ? `: ${cause}` : '';
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

    const rawHost = options.host
        ?? process.env.MCP_AUTOMATION_WS_HOST
        ?? process.env.MCP_AUTOMATION_HOST
        ?? DEFAULT_AUTOMATION_HOST;
    const host = normalizeHost(rawHost, 'Automation bridge host', allowNonLoopback, log);
    // Explicit options or environment always win. The project config is only a
    // fallback so a per-project Kilo entry needs nothing but UE_PROJECT_PATH.
    const hasExplicitPort = options.port !== undefined
        || options.ports !== undefined
        || options.clientPort !== undefined
        || process.env.MCP_AUTOMATION_CLIENT_PORT !== undefined
        || process.env.MCP_AUTOMATION_WS_PORT !== undefined
        || process.env.MCP_AUTOMATION_PORT !== undefined
        || process.env.MCP_AUTOMATION_WS_PORTS !== undefined;
    const defaultPort = sanitizePort(options.port)
        ?? sanitizePort(process.env.MCP_AUTOMATION_WS_PORT)
        ?? sanitizePort(process.env.MCP_AUTOMATION_PORT)
        ?? (hasExplicitPort ? null : readProjectListenPort(log))
        ?? DEFAULT_AUTOMATION_PORT;
    const ports = resolvePorts(options.ports, defaultPort);
    const packageInfo = readPackageInfo(log);
    const heartbeatIntervalMs = (options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS) > 0
        ? (options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS)
        : 0;
    const rawClientHost = options.clientHost
        ?? process.env.MCP_AUTOMATION_CLIENT_HOST
        ?? host;

    return {
        host,
        port: ports[0] ?? DEFAULT_AUTOMATION_PORT,
        ports,
        negotiatedProtocols: resolveProtocols(options.protocols),
        capabilityToken: options.capabilityToken ?? process.env.MCP_AUTOMATION_CAPABILITY_TOKEN ?? undefined,
        enabled: options.enabled ?? process.env.MCP_AUTOMATION_BRIDGE_ENABLED !== 'false',
        serverName: options.serverName ?? process.env.MCP_SERVER_NAME ?? packageInfo.name ?? 'unreal-engine-mcp',
        serverVersion: options.serverVersion
            ?? process.env.MCP_SERVER_VERSION
            ?? packageInfo.version
            ?? process.env.npm_package_version
            ?? '0.0.0',
        clientHost: normalizeHost(rawClientHost, 'Automation bridge client host', allowNonLoopback, log),
        clientPort: options.clientPort ?? sanitizePort(process.env.MCP_AUTOMATION_CLIENT_PORT) ?? defaultPort,
        serverLegacyEnabled: options.serverLegacyEnabled ?? process.env.MCP_AUTOMATION_SERVER_LEGACY !== 'false',
        maxConcurrentConnections: Math.max(1, options.maxConcurrentConnections ?? 10),
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
        ),
        maxInboundAutomationRequestsPerMinute: parseNonNegativeInt(
            options.maxInboundAutomationRequestsPerMinute ?? process.env.MCP_AUTOMATION_MAX_AUTOMATION_REQUESTS_PER_MINUTE,
            DEFAULT_MAX_INBOUND_AUTOMATION_REQUESTS_PER_MINUTE
        )
    };
}

function resolvePorts(optionPorts: number[] | undefined, defaultPort: number): number[] {
    const configuredPortValues: Array<number | string> | undefined = optionPorts
        ? optionPorts
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

    return Array.from(new Set(sanitizedPorts));
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

function readPackageInfo(log: BridgeConfigLogger): PackageInfo {
    try {
        const loaded: unknown = requirePackage('../../package.json');
        return parsePackageInfo(loaded);
    } catch (error) {
        log.debug('Unable to read package.json for version info', error instanceof Error ? error : String(error));
        return {};
    }
}

function parsePackageInfo(value: unknown): PackageInfo {
    if (!value || typeof value !== 'object') {
        return {};
    }

    const record = value as Record<string, unknown>;
    return {
        name: typeof record.name === 'string' ? record.name : undefined,
        version: typeof record.version === 'string' ? record.version : undefined
    };
}
