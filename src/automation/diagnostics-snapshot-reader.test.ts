import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import * as reader from './diagnostics-snapshot-reader.js';
import { AutomationLogger } from './log-redaction.js';

// Todo 9 (BB-005) lane 1 — read-only diagnostics snapshot reader.
//
// These tests drive the REAL module against a real temp Saved tree (the
// module-level driver shape required by the lane): valid current/previous
// parse, corrupt/oversized snapshots fail closed, unknown file fields are
// silently dropped, and the exported API surface makes writing impossible.

const VALID_CURRENT = {
    schemaVersion: 1,
    instance: { instanceId: 'a1b2c3d4', pid: 12345, startTimeUtc: '2026-08-12T11:05:00.000Z' },
    counters: { requests: 3, failures: 1, refusals: 1, queueWaitMs: 42 },
    lastRequest: {
        requestId: 'req-7f3a',
        correlationId: 'corr-9c21',
        canonicalAction: 'manage_asset.import_asset',
        origin: 'WebSocket',
        queueDepth: 2,
        enqueueAt: '2026-08-12T11:05:00.000Z',
        dispatchAt: '2026-08-12T11:05:00.120Z',
        terminalAt: '2026-08-12T11:05:00.500Z',
        terminalClass: 'success',
    },
    lastHandshake: { at: '2026-08-12T11:04:59.000Z', ok: true },
    lastDisconnect: null,
    session: { created: 1, closed: 0, active: 1, lastIdentitySha256: '9f86d081884c7d65', at: '2026-08-12T11:05:00.100Z' },
};

const VALID_PREVIOUS = {
    schemaVersion: 1,
    instance: { instanceId: 'deadbeef', pid: 9999, startTimeUtc: '2026-08-11T22:00:00.000Z' },
    counters: { requests: 1, failures: 0, refusals: 0, queueWaitMs: 7 },
    lastRequest: {
        requestId: 'req-crashed',
        correlationId: 'corr-crashed',
        canonicalAction: 'control_actor.spawn_actor',
        origin: 'NativeHTTP',
        queueDepth: 0,
        enqueueAt: '2026-08-11T22:00:01.000Z',
        dispatchAt: '2026-08-11T22:00:01.010Z',
        terminalAt: null,
        terminalClass: null,
    },
    lastHandshake: null,
    lastDisconnect: { at: '2026-08-11T22:00:02.000Z', reason: 'closed' },
    session: null,
};

function diagnosticsDir(projectRoot: string): string {
    const dir = join(projectRoot, 'Saved', 'MCP', 'diagnostics');
    mkdirSync(dir, { recursive: true });
    return dir;
}

function writeSnapshot(projectRoot: string, name: 'current-session.json' | 'previous-session.json', value: unknown): void {
    writeFileSync(join(diagnosticsDir(projectRoot), name), JSON.stringify(value), { encoding: 'utf8' });
}

describe('DiagnosticsSnapshotReader', () => {
    let projectRoot: string;
    let warnSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        projectRoot = mkdtempSync(join(tmpdir(), 'mcp-diagnostics-reader-'));
        warnSpy = vi.spyOn(AutomationLogger.prototype, 'warn').mockImplementation(() => {});
    });

    afterEach(() => {
        rmSync(projectRoot, { recursive: true, force: true });
        vi.unstubAllEnvs();
        vi.restoreAllMocks();
    });

    it('parses valid current and previous snapshots from the diagnostics dir', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeSnapshot(projectRoot, 'current-session.json', VALID_CURRENT);
        writeSnapshot(projectRoot, 'previous-session.json', VALID_PREVIOUS);

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));

        expect(result.current?.schemaVersion).toBe(1);
        expect(result.current?.instance.pid).toBe(12345);
        expect(result.current?.lastRequest?.canonicalAction).toBe('manage_asset.import_asset');
        expect(result.current?.lastRequest?.terminalClass).toBe('success');
        expect(result.current?.session?.lastIdentitySha256).toBe('9f86d081884c7d65');

        expect(result.previous?.instance.pid).toBe(9999);
        expect(result.previous?.lastRequest?.requestId).toBe('req-crashed');
        expect(result.previous?.lastRequest?.terminalClass).toBeNull();
        expect(result.previous?.lastDisconnect?.reason).toBe('closed');
    });

    it('resolves a .uproject file path to its directory', async () => {
        writeFileSync(join(projectRoot, 'MyGame.uproject'), '{}', { encoding: 'utf8' });
        vi.stubEnv('UE_PROJECT_PATH', join(projectRoot, 'MyGame.uproject'));
        writeSnapshot(projectRoot, 'current-session.json', VALID_CURRENT);

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current?.instance.pid).toBe(12345);
    });

    it('returns null for missing files without throwing', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current).toBeNull();
        expect(result.previous).toBeNull();
    });

    it('fails closed on corrupt JSON with one bounded warning', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeFileSync(join(diagnosticsDir(projectRoot), 'current-session.json'), '{ not json', { encoding: 'utf8' });
        writeFileSync(join(diagnosticsDir(projectRoot), 'previous-session.json'), '[]', { encoding: 'utf8' });

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current).toBeNull();
        expect(result.previous).toBeNull();
        expect(warnSpy).toHaveBeenCalled();
        // The warning names the path, never the contents.
        for (const call of warnSpy.mock.calls) {
            expect(call.map(String).join(' ')).not.toContain('not json');
        }
    });

    it('fails closed on an oversized snapshot (greater than 64 KiB)', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        const oversized = { ...VALID_CURRENT, counters: { requests: 'x'.repeat(70_000) } };
        writeSnapshot(projectRoot, 'current-session.json', oversized);

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current).toBeNull();
        expect(warnSpy).toHaveBeenCalled();
    });

    it('warns at most once per file kind per process', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeFileSync(join(diagnosticsDir(projectRoot), 'current-session.json'), '{ bad', { encoding: 'utf8' });

        await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(warnSpy).toHaveBeenCalledTimes(1);
    });

    it('silently drops unknown file fields instead of passing them through', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        const tampered = {
            ...VALID_CURRENT,
            payload: { code: 'evil' },
            capabilityToken: 'super-secret',
            idempotencyKey: 'raw-key',
            extraNested: { sessionId: 'raw-session' },
        };
        writeSnapshot(projectRoot, 'current-session.json', tampered);

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current?.lastRequest?.canonicalAction).toBe('manage_asset.import_asset');
        expect(result.current).not.toHaveProperty('payload');
        expect(result.current).not.toHaveProperty('capabilityToken');
        expect(result.current).not.toHaveProperty('idempotencyKey');
        expect(result.current).not.toHaveProperty('extraNested');
        // The dropped secrets never reach the log.
        for (const call of warnSpy.mock.calls) {
            expect(call.map(String).join(' ')).not.toContain('super-secret');
            expect(call.map(String).join(' ')).not.toContain('raw-key');
        }
    });

    it('rejects an unknown schemaVersion as corrupt', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeSnapshot(projectRoot, 'current-session.json', { ...VALID_CURRENT, schemaVersion: 2 });

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current).toBeNull();
    });

    it('rejects a non-object snapshot shape', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeSnapshot(projectRoot, 'current-session.json', 'just a string');

        const result = await reader.readDiagnosticsSnapshots(new AutomationLogger('test'));
        expect(result.current).toBeNull();
    });

    it('exports no write API — the module surface is read-only', async () => {
        const exportNames = Object.keys(reader);
        for (const name of exportNames) {
            expect(name).not.toMatch(/write|create|append|save|put|delete|remove|update/i);
        }
        expect(exportNames).toContain('readDiagnosticsSnapshots');
        expect(exportNames).toContain('MAX_SNAPSHOT_BYTES');
        expect(reader.MAX_SNAPSHOT_BYTES).toBe(64 * 1024);
    });
});
