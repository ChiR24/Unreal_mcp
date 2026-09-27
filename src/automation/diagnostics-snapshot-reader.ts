import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { z } from 'zod';
import { projectRootFromEnv } from '../utils/config/ini-reader.js';
import type { Logger } from '../utils/logging/logger.js';

// Read-only diagnostics snapshot reader.
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

const nullableString = z.string().nullable();
const count = z.number();
// The allowlist: z.object strips every key not named here, and a missing or
// mistyped field fails the whole snapshot closed.
const SnapshotSchema = z.object({
    schemaVersion: z.literal(SNAPSHOT_SCHEMA_VERSION),
    instance: z.object({ instanceId: z.string(), pid: count, startTimeUtc: z.string() }),
    counters: z.object({ requests: count, failures: count, refusals: count, queueWaitMs: count }),
    lastRequest: z.object({
        requestId: nullableString,
        correlationId: nullableString,
        canonicalAction: nullableString,
        origin: nullableString,
        queueDepth: count,
        enqueueAt: nullableString,
        dispatchAt: nullableString,
        terminalAt: nullableString,
        terminalClass: nullableString,
    }),
    lastHandshake: z.object({ at: nullableString, ok: z.boolean() }).nullish().transform((v) => v ?? null),
    lastDisconnect: z.object({ at: nullableString, reason: nullableString }).nullish().transform((v) => v ?? null),
    session: z.object({
        created: count,
        closed: count,
        active: count,
        lastIdentitySha256: nullableString,
        at: nullableString,
    }).nullish().transform((v) => v ?? null),
});

export type DiagnosticsSnapshotSummary = z.output<typeof SnapshotSchema>;

export interface DiagnosticsSnapshotsResult {
    current: DiagnosticsSnapshotSummary | null;
    previous: DiagnosticsSnapshotSummary | null;
}

// One warning per file and failure reason per process, so a polled resource
// re-reading the same bad file cannot flood the log.
const warnedFor = new Set<string>();

function warnOnce(log: Logger, fileName: string, reason: string, filePath: string): void {
    const key = `${fileName}:${reason}`;
    if (warnedFor.has(key)) {
        return;
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
            warnOnce(log, fileName, 'unreadable', filePath);
        }
        return null;
    }

    if (Buffer.byteLength(content, 'utf8') > MAX_SNAPSHOT_BYTES) {
        warnOnce(log, fileName, 'oversized', filePath);
        return null;
    }

    let parsed: unknown;
    try {
        parsed = JSON.parse(content) as unknown;
    } catch {
        warnOnce(log, fileName, 'corrupt', filePath);
        return null;
    }

    const summary = SnapshotSchema.safeParse(parsed);
    if (!summary.success) {
        warnOnce(log, fileName, 'corrupt', filePath);
        return null;
    }
    return summary.data;
}

export async function readDiagnosticsSnapshots(log: Logger): Promise<DiagnosticsSnapshotsResult> {
    const projectRoot = projectRootFromEnv();
    if (projectRoot === undefined) {
        return { current: null, previous: null };
    }
    const dir = join(projectRoot, 'Saved', 'MCP', 'diagnostics');
    // Two independent files: read them together rather than one after the other.
    const [current, previous] = await Promise.all([
        readSnapshotFile(log, 'current-session.json', dir),
        readSnapshotFile(log, 'previous-session.json', dir)
    ]);
    return { current, previous };
}
