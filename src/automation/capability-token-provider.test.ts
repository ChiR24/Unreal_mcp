import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { AutomationLogger } from './log-redaction.js';
import { CapabilityTokenProvider } from './capability-token-provider.js';

const VALID_TOKEN = 'a'.repeat(64);

function tokenFileDir(projectRoot: string): string {
    const dir = join(projectRoot, 'Saved', 'MCP');
    mkdirSync(dir, { recursive: true });
    return dir;
}

function writeToken(projectRoot: string, content: string): string {
    const file = join(tokenFileDir(projectRoot), 'capability-token');
    writeFileSync(file, content, { encoding: 'utf8' });
    return file;
}

describe('CapabilityTokenProvider', () => {
    let projectRoot: string;
    let warnSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        projectRoot = mkdtempSync(join(tmpdir(), 'cap-token-provider-'));
        warnSpy = vi.spyOn(AutomationLogger.prototype, 'warn').mockImplementation(() => {});
    });

    afterEach(() => {
        rmSync(projectRoot, { recursive: true, force: true });
        vi.unstubAllEnvs();
        vi.restoreAllMocks();
    });

    it('reads a 64-hex token file with no trailing newline', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeToken(projectRoot, VALID_TOKEN);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBe(VALID_TOKEN);
    });

    it('tolerates a trailing newline in the token file', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeToken(projectRoot, `${VALID_TOKEN}\n`);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBe(VALID_TOKEN);
    });

    it('resolves a .uproject file path to its directory', async () => {
        const uproject = join(projectRoot, 'MyGame.uproject');
        writeFileSync(uproject, '{}', { encoding: 'utf8' });
        vi.stubEnv('UE_PROJECT_PATH', uproject);
        writeToken(projectRoot, VALID_TOKEN);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBe(VALID_TOKEN);
    });

    it('prefers the explicit token over the env var and file', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        vi.stubEnv('MCP_AUTOMATION_CAPABILITY_TOKEN', 'b'.repeat(64));
        writeToken(projectRoot, VALID_TOKEN);

        const provider = new CapabilityTokenProvider('c'.repeat(64), new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBe('c'.repeat(64));
    });

    it('prefers the env var over the file', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        vi.stubEnv('MCP_AUTOMATION_CAPABILITY_TOKEN', 'b'.repeat(64));
        writeToken(projectRoot, VALID_TOKEN);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBe('b'.repeat(64));
    });

    it('returns undefined and warns when the token file is missing', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBeUndefined();

        const warnedPath = join(projectRoot, 'Saved', 'MCP', 'capability-token');
        const warnMessage = warnSpy.mock.calls[0]?.map(String).join(' ') ?? '';
        expect(warnMessage).toContain(warnedPath);
    });

    it('returns undefined and warns when the token file is empty', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeToken(projectRoot, '  \n');

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBeUndefined();
        expect(warnSpy).toHaveBeenCalled();
    });

    it('returns undefined when the token file is malformed (not 64 hex)', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeToken(projectRoot, 'not-a-valid-token');

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBeUndefined();
        expect(warnSpy).toHaveBeenCalled();
    });

    it('returns undefined and warns exactly once when UE_PROJECT_PATH is unset', async () => {
        vi.stubEnv('UE_PROJECT_PATH', '');

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBeUndefined();
        // One diagnostic warning, never one per handshake retry.
        expect(warnSpy).toHaveBeenCalledTimes(1);

        await provider.resolve();
        expect(warnSpy).toHaveBeenCalledTimes(1);
    });

    it('re-reads the file on every call (picks up a token created later)', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await expect(provider.resolve()).resolves.toBeUndefined();

        writeToken(projectRoot, VALID_TOKEN);
        await expect(provider.resolve()).resolves.toBe(VALID_TOKEN);
    });

    it('never logs the token value', async () => {
        vi.stubEnv('UE_PROJECT_PATH', projectRoot);
        writeToken(projectRoot, VALID_TOKEN);

        const provider = new CapabilityTokenProvider(undefined, new AutomationLogger('test'));
        await provider.resolve();

        const calls = warnSpy.mock.calls.flat().map(String);
        for (const call of calls) {
            expect(call).not.toContain(VALID_TOKEN);
        }
    });
});
