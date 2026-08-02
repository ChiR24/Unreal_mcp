import { readFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import type { Logger } from '../utils/logging/logger.js';

/**
 * Shared token-file contract (byte-identical with C++ side):
 * - Path: project root from `process.env.UE_PROJECT_PATH` (ends with `.uproject` → `path.dirname`;
 *   unset → no file check, resolve undefined); then `path.join(projectRoot, 'Saved', 'MCP', 'capability-token')`.
 * - Format: raw UTF-8, 64 lowercase hex chars, possibly trailing newline — `trim()` before use.
 * - Resolution order: 1) explicit `options.capabilityToken`; 2) env `MCP_AUTOMATION_CAPABILITY_TOKEN`;
 *   3) token file. TS is READ-ONLY for the file (C++ is sole generator).
 * - Re-check the file on EVERY resolve() call (cheap small read; editor/plugin may start AFTER the TS
 *   server — handshake retries must pick it up). Resolution happens at hello time, not startup.
 * - Fail-closed: missing/unreadable/malformed/empty → resolve undefined; handshake sends no token;
 *   plugin (default on) refuses. Warnings are one-shot so handshake retries never spam the log.
 * - SECRET HYGIENE: never log the token value; keep out of diagnostics/status.
 */
export class CapabilityTokenProvider {
    private readonly log: Logger;
    private warnedAboutMissingProjectPath = false;
    private warnedAboutFileIssue = false;

    /**
     * @param explicitToken - explicit token from options (highest priority, skip file check)
     * @param log - logger for warning on malformed/missing file
     */
    constructor(
        private readonly explicitToken: string | undefined,
        log: Logger
    ) {
        this.log = log;
    }

    /**
     * Resolve the capability token.
     * Resolution order: explicit token → env → token file.
     * File is re-read on every call (no caching).
     */
    async resolve(): Promise<string | undefined> {
        // Priority 1: explicit token from options
        if (this.explicitToken !== undefined && this.explicitToken.length > 0) {
            return this.explicitToken;
        }

        // Priority 2: env var
        const envToken = process.env.MCP_AUTOMATION_CAPABILITY_TOKEN;
        if (envToken !== undefined && envToken.length > 0) {
            return envToken;
        }

        // Priority 3: token file
        return this.resolveFromFile();
    }

    private async resolveFromFile(): Promise<string | undefined> {
        const projectRoot = this.resolveProjectRoot();
        if (projectRoot === undefined) {
            // Warn exactly once: handshake retries would otherwise repeat the
            // same diagnostic on every attempt. Without UE_PROJECT_PATH the
            // token file cannot be located, and a plugin that requires a
            // capability token (the default) refuses the handshake.
            if (!this.warnedAboutMissingProjectPath) {
                this.warnedAboutMissingProjectPath = true;
                this.log.warn('[CapabilityTokenProvider] UE_PROJECT_PATH is not set; cannot locate the capability-token file. If the plugin requires a capability token (default), the handshake will be refused.');
            }
            return undefined;
        }

        const filePath = join(projectRoot, 'Saved', 'MCP', 'capability-token');

        let content: string;
        try {
            content = await readFile(filePath, { encoding: 'utf8' });
        } catch {
            // Fail-closed: missing/unreadable file → undefined
            if (!this.warnedAboutFileIssue) {
                this.warnedAboutFileIssue = true;
                this.log.warn(`[CapabilityTokenProvider] Token file not readable: ${filePath}`);
            }
            return undefined;
        }

        const trimmed = content.toLowerCase().trim();

        // Fail-closed: empty → undefined
        if (trimmed.length === 0) {
            if (!this.warnedAboutFileIssue) {
                this.warnedAboutFileIssue = true;
                this.log.warn(`[CapabilityTokenProvider] Token file is empty: ${filePath}`);
            }
            return undefined;
        }

        // Validate format: exactly 64 lowercase hex chars
        if (!/^[0-9a-f]{64}$/.test(trimmed)) {
            if (!this.warnedAboutFileIssue) {
                this.warnedAboutFileIssue = true;
                this.log.warn(`[CapabilityTokenProvider] Token file has invalid format (expected 64 lowercase hex chars): ${filePath}`);
            }
            return undefined;
        }

        return trimmed;
    }

    private resolveProjectRoot(): string | undefined {
        const ueProjectPath = process.env.UE_PROJECT_PATH;
        if (ueProjectPath === undefined || ueProjectPath.length === 0) {
            return undefined;
        }

        // If it looks like a .uproject file, use its directory
        if (ueProjectPath.endsWith('.uproject')) {
            return dirname(ueProjectPath);
        }

        return ueProjectPath;
    }
}
