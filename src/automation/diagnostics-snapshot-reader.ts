import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import type { Logger } from '../utils/logging/logger.js';

// Todo 9 (BB-005) lane 1 — read-only diagnostics snapshot reader.
//
// Mirrors the plugin store contract (McpDiagnosticsSnapshotSchema.h): the
// plugin is the SOLE writer of <Project>/Saved/MCP/diagnostics/current-session.json
// and previous-session.json (64 KiB max each). This module only PARSES them.
// There is no write/create/append/save/delete export anywhere on this surface,
// so a write from the TypeScript side is impossible by API shape.
//
// Strict allowlist projection: only known, bounded fields are copied out of the
// parsed file; unknown fields (payload, code, capability tokens, idempotency
// keys, paths, principals, raw session ids) are silently dropped — never passed
// through to a resource body. Corrupt, oversized, or malformed snapshots fail
// closed (null) with ONE warning per distinct failure per process that names
// the path only, never contents, so a polled resource cannot flood the log.

export const MAX_SNAPSHOT_BYTES = 64 * 1024;
export const SNAPSHOT_SCHEMA_VERSION = 1;

export interface DiagnosticsInstanceSummary {
    instanceId: string;
    pid: number;
    startTimeUtc: string;
}

export interface DiagnosticsCountersSummary {
    requests: number;
    failures: number;
    refusals: number;
    queueWaitMs: number;
}

export interface DiagnosticsLastRequestSummary {
    requestId: string | null;
    correlationId: string | null;
    canonicalAction: string | null;
    origin: string | null;
    queueDepth: number;
    enqueueAt: string | null;
    dispatchAt: string | null;
    terminalAt: string | null;
    terminalClass: string | null;
}

export interface DiagnosticsHandshakeSummary {
    at: string | null;
    ok: boolean;
}

export interface DiagnosticsDisconnectSummary {
    at: string | null;
    reason: string | null;
}

export interface DiagnosticsSessionSummary {
    created: number;
    closed: number;
    active: number;
    lastIdentitySha256: string | null;
    at: string | null;
}

export interface DiagnosticsSnapshotSummary {
    schemaVersion: number;
    instance: DiagnosticsInstanceSummary;
    counters: DiagnosticsCountersSummary;
    lastRequest: DiagnosticsLastRequestSummary;
    lastHandshake: DiagnosticsHandshakeSummary | null;
    lastDisconnect: DiagnosticsDisconnectSummary | null;
    session: DiagnosticsSessionSummary | null;
}

export interface DiagnosticsSnapshotsResult {
    current: DiagnosticsSnapshotSummary | null;
    previous: DiagnosticsSnapshotSummary | null;
}

type JsonRecord = Record<string, unknown>;

function asRecord(value: unknown): JsonRecord | undefined {
    return value !== null && typeof value === 'object' && !Array.isArray(value)
        ? (value as JsonRecord)
        : undefined;
}

function asString(value: unknown): string | undefined {
    return typeof value === 'string' ? value : undefined;
}

function asFiniteNumber(value: unknown): number | undefined {
    return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function asBoolean(value: unknown): boolean | undefined {
    return typeof value === 'boolean' ? value : undefined;
}

function asNullableString(value: unknown): string | null | undefined {
    return value === null ? null : asString(value);
}

function asNullableRecord(value: unknown): JsonRecord | null | undefined {
    return value === null ? null : asRecord(value);
}

// Strict allowlist projection. A wrong type, a non-object root, or a schema
// version mismatch fails the whole snapshot closed (returns null).
function projectSnapshot(value: unknown): DiagnosticsSnapshotSummary | null {
    const root = asRecord(value);
    if (root === undefined || asFiniteNumber(root.schemaVersion) !== SNAPSHOT_SCHEMA_VERSION) {
        return null;
    }

    const instance = asRecord(root.instance);
    const counters = asRecord(root.counters);
    const lastRequest = asRecord(root.lastRequest);
    if (instance === undefined || counters === undefined || lastRequest === undefined) {
        return null;
    }

    const instanceId = asString(instance.instanceId);
    const pid = asFiniteNumber(instance.pid);
    const startTimeUtc = asString(instance.startTimeUtc);
    const requests = asFiniteNumber(counters.requests);
    const failures = asFiniteNumber(counters.failures);
    const refusals = asFiniteNumber(counters.refusals);
    const queueWaitMs = asFiniteNumber(counters.queueWaitMs);
    const requestId = asNullableString(lastRequest.requestId);
    const correlationId = asNullableString(lastRequest.correlationId);
    const canonicalAction = asNullableString(lastRequest.canonicalAction);
    const origin = asNullableString(lastRequest.origin);
    const queueDepth = asFiniteNumber(lastRequest.queueDepth);
    const enqueueAt = asNullableString(lastRequest.enqueueAt);
    const dispatchAt = asNullableString(lastRequest.dispatchAt);
    const terminalAt = asNullableString(lastRequest.terminalAt);
    const terminalClass = asNullableString(lastRequest.terminalClass);
    if (
        instanceId === undefined ||
        pid === undefined ||
        startTimeUtc === undefined ||
        requests === undefined ||
        failures === undefined ||
        refusals === undefined ||
        queueWaitMs === undefined ||
        requestId === undefined ||
        correlationId === undefined ||
        canonicalAction === undefined ||
        origin === undefined ||
        queueDepth === undefined ||
        enqueueAt === undefined ||
        dispatchAt === undefined ||
        terminalAt === undefined ||
        terminalClass === undefined
    ) {
        return null;
    }

    let lastHandshake: DiagnosticsHandshakeSummary | null = null;
    const handshakeRec = asNullableRecord(root.lastHandshake);
    if (handshakeRec !== undefined && handshakeRec !== null) {
        const at = asNullableString(handshakeRec.at);
        const ok = asBoolean(handshakeRec.ok);
        if (at === undefined || ok === undefined) {
            return null;
        }
        lastHandshake = { at, ok };
    }

    let lastDisconnect: DiagnosticsDisconnectSummary | null = null;
    const disconnectRec = asNullableRecord(root.lastDisconnect);
    if (disconnectRec !== undefined && disconnectRec !== null) {
        const at = asNullableString(disconnectRec.at);
        const reason = asNullableString(disconnectRec.reason);
        if (at === undefined || reason === undefined) {
            return null;
        }
        lastDisconnect = { at, reason };
    }

    let session: DiagnosticsSessionSummary | null = null;
    const sessionRec = asNullableRecord(root.session);
    if (sessionRec !== undefined && sessionRec !== null) {
        const created = asFiniteNumber(sessionRec.created);
        const closed = asFiniteNumber(sessionRec.closed);
        const active = asFiniteNumber(sessionRec.active);
        const lastIdentitySha256 = asNullableString(sessionRec.lastIdentitySha256);
        const at = asNullableString(sessionRec.at);
        if (created === undefined || closed === undefined || active === undefined || lastIdentitySha256 === undefined || at === undefined) {
            return null;
        }
        session = { created, closed, active, lastIdentitySha256, at };
    }

    return {
        schemaVersion: SNAPSHOT_SCHEMA_VERSION,
        instance: { instanceId, pid, startTimeUtc },
        counters: { requests, failures, refusals, queueWaitMs },
        lastRequest: {
            requestId,
            correlationId,
            canonicalAction,
            origin,
            queueDepth,
            enqueueAt,
            dispatchAt,
            terminalAt,
            terminalClass,
        },
        lastHandshake,
        lastDisconnect,
        session,
    };
}

function resolveProjectRoot(): string | undefined {
    const ueProjectPath = process.env.UE_PROJECT_PATH;
    if (ueProjectPath === undefined || ueProjectPath.length === 0) {
        return undefined;
    }
    return ueProjectPath.endsWith('.uproject') ? dirname(ueProjectPath) : ueProjectPath;
}

// One warning per distinct failure per process: the dedup key includes a digest
// of the file content, so a polled resource re-reading the same corrupt file
// cannot flood the log while a genuinely new failure still surfaces once. The
// digest makes the key space unbounded (a file that keeps changing yields a fresh
// digest per poll), so the set is capped; clearing it only risks a duplicate
// warning, never a leak.
const WARNED_FOR_MAX_ENTRIES = 256;
const warnedFor = new Set<string>();

function warnOnce(log: Logger, fileName: string, reason: string, filePath: string, content: string): void {
    const key = `${fileName}:${reason}:${createHash('sha256').update(content, 'utf8').digest('hex')}`;
    if (warnedFor.has(key)) {
        return;
    }
    if (warnedFor.size >= WARNED_FOR_MAX_ENTRIES) {
        warnedFor.clear();
    }
    warnedFor.add(key);
    log.warn(`[DiagnosticsSnapshotReader] Ignoring ${reason} diagnostics snapshot: ${filePath}`);
}

async function readSnapshotFile(
    log: Logger,
    fileName: string,
    dir: string
): Promise<DiagnosticsSnapshotSummary | null> {
    const filePath = join(dir, fileName);
    let content: string;
    try {
        content = await readFile(filePath, { encoding: 'utf8' });
    } catch (error) {
        // A missing file is a normal fresh-install state, not a failure.
        if ((error as NodeJS.ErrnoException).code !== 'ENOENT') {
            warnOnce(log, fileName, 'unreadable', filePath, '');
        }
        return null;
    }

    if (Buffer.byteLength(content, 'utf8') > MAX_SNAPSHOT_BYTES) {
        warnOnce(log, fileName, 'oversized', filePath, content);
        return null;
    }

    let parsed: unknown;
    try {
        parsed = JSON.parse(content) as unknown;
    } catch {
        warnOnce(log, fileName, 'corrupt', filePath, content);
        return null;
    }

    const summary = projectSnapshot(parsed);
    if (summary === null) {
        warnOnce(log, fileName, 'corrupt', filePath, content);
        return null;
    }
    return summary;
}

export async function readDiagnosticsSnapshots(log: Logger): Promise<DiagnosticsSnapshotsResult> {
    const projectRoot = resolveProjectRoot();
    if (projectRoot === undefined) {
        return { current: null, previous: null };
    }
    const dir = join(projectRoot, 'Saved', 'MCP', 'diagnostics');
    const current = await readSnapshotFile(log, 'current-session.json', dir);
    const previous = await readSnapshotFile(log, 'previous-session.json', dir);
    return { current, previous };
}
