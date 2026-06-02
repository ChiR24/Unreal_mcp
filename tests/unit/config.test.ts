import { describe, it, expect, vi, afterEach } from 'vitest';
import { EnvSchema } from '../../src/config.js';
import { getRequiredComponent, resolveClassAlias } from '../../src/config/class-aliases.js';
import { loadEnv } from '../../src/types/env.js';

describe('EnvSchema env var defaults (Zod v4 compatibility)', () => {
    it('parse({}) returns documented defaults for preprocessed boolean fields', () => {
        const result = EnvSchema.parse({});
        expect(result.MCP_ROUTE_STDOUT_LOGS).toBe(true);
        expect(result.MCP_AUTOMATION_CLIENT_MODE).toBe(false);
    });

    it('parse({}) returns documented defaults for preprocessed number fields', () => {
        const result = EnvSchema.parse({});
        expect(result.MCP_AUTOMATION_PORT).toBe(8091);
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(30000);
    });

    it('parse({}) returns documented defaults for plain (non-preprocessed) fields', () => {
        const result = EnvSchema.parse({});
        expect(result.NODE_ENV).toBe('development');
        expect(result.LOG_LEVEL).toBe('debug');
        expect(result.MCP_AUTOMATION_HOST).toBe('127.0.0.1');
        expect(result.MCP_DEFAULT_CATEGORIES).toBe('all');
        expect(result.MCP_ADDITIONAL_PATH_PREFIXES).toBe('');
    });

    it('respects user-set boolean strings ("true"/"false")', () => {
        const result = EnvSchema.parse({
            MCP_ROUTE_STDOUT_LOGS: 'false',
            MCP_AUTOMATION_CLIENT_MODE: 'true',
        });
        expect(result.MCP_ROUTE_STDOUT_LOGS).toBe(false);
        expect(result.MCP_AUTOMATION_CLIENT_MODE).toBe(true);
    });

    it('respects user-set number strings', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: '3000',
            MCP_CONNECTION_TIMEOUT_MS: '1000',
            MCP_REQUEST_TIMEOUT_MS: '60000',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(3000);
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(1000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(60000);
    });

    it('maps legacy timeout aliases into canonical schema fields', () => {
        const result = EnvSchema.parse({
            UNREAL_CONNECTION_TIMEOUT: '7000',
            MCP_AUTOMATION_REQUEST_TIMEOUT_MS: '90000',
        });

        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(7000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(90000);
    });

    it('prefers canonical timeout env vars over legacy aliases', () => {
        const result = EnvSchema.parse({
            MCP_CONNECTION_TIMEOUT_MS: '8000',
            UNREAL_CONNECTION_TIMEOUT: '7000',
            MCP_REQUEST_TIMEOUT_MS: '100000',
            MCP_AUTOMATION_REQUEST_TIMEOUT_MS: '90000',
        });

        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(8000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(100000);
    });

    it('partial input mixes user values and defaults', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: '7777',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(7777);
        expect(result.MCP_ROUTE_STDOUT_LOGS).toBe(true);
        expect(result.MCP_AUTOMATION_CLIENT_MODE).toBe(false);
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(30000);
    });

    it('garbage boolean strings fall back to false (preprocess contract)', () => {
        const result = EnvSchema.parse({
            MCP_ROUTE_STDOUT_LOGS: 'not-a-bool',
        });
        expect(result.MCP_ROUTE_STDOUT_LOGS).toBe(false);
    });

    it('non-numeric port strings fall back to documented default', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: 'abc',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(8091);
    });

    it('partially numeric strings fall back to documented defaults', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: '8092abc',
            MCP_CONNECTION_TIMEOUT_MS: '5000ms',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(8091);
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
    });

    it('non-decimal numeric strings fall back to documented defaults', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: '0x1f9b',
            MCP_REQUEST_TIMEOUT_MS: '3e4',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(8091);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(30000);
    });

    it('negative, zero, and fractional numeric values fall back to documented defaults', () => {
        const result = EnvSchema.parse({
            MCP_AUTOMATION_PORT: '-1',
            MCP_CONNECTION_TIMEOUT_MS: '0',
            MCP_REQUEST_TIMEOUT_MS: '1000.5',
        });
        expect(result.MCP_AUTOMATION_PORT).toBe(8091);
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
        expect(result.MCP_REQUEST_TIMEOUT_MS).toBe(30000);
    });

    it('deduplicates and validates additional path prefixes', async () => {
        const originalEnv = process.env;
        process.env = {
            ...originalEnv,
            MCP_ADDITIONAL_PATH_PREFIXES: 'ProjectObject,/ProjectObject/,../Bad,/Plugin//Bad,/ProjectAnimation'
        };
        vi.resetModules();

        const mod = await import('../../src/config.js');

        expect(mod.getAdditionalPathPrefixes()).toEqual(['/ProjectObject/', '/ProjectAnimation/']);
        process.env = originalEnv;
        vi.resetModules();
    });
});

describe('class aliases', () => {
    it('resolves aliases case-insensitively without changing unknown class paths', () => {
        expect(resolveClassAlias('pointlight')).toBe('/Script/Engine.PointLight');
        expect(resolveClassAlias(' SplineActor ')).toBe('/Script/Engine.Actor');
        expect(resolveClassAlias('/Script/Engine.Actor')).toBe('/Script/Engine.Actor');
    });

    it('detects component requirements case-insensitively', () => {
        expect(getRequiredComponent('splineactor')).toBe('SplineComponent');
        expect(getRequiredComponent(' Spline ')).toBe('SplineComponent');
        expect(getRequiredComponent('PointLight')).toBeUndefined();
    });
});

describe('loadEnv', () => {
    const originalEnv = process.env;

    afterEach(() => {
        process.env = originalEnv;
    });

    it('normalizes blank optional env vars to undefined', () => {
        process.env = {
            ...originalEnv,
            UE_PROJECT_PATH: '   ',
            UE_EDITOR_EXE: '',
            UE_SCREENSHOT_DIR: '/tmp/screenshots',
        };

        expect(loadEnv()).toEqual({
            UE_PROJECT_PATH: undefined,
            UE_EDITOR_EXE: undefined,
            UE_SCREENSHOT_DIR: '/tmp/screenshots',
        });
    });
});

describe('config module load (regression: src/config.ts must not throw on empty env)', () => {
    const originalEnv = process.env;

    afterEach(() => {
        process.env = originalEnv;
        vi.resetModules();
    });

    it('importing src/config.js with no affected env vars set yields documented defaults', async () => {
        process.env = { ...originalEnv };
        delete process.env.MCP_ROUTE_STDOUT_LOGS;
        delete process.env.MCP_AUTOMATION_PORT;
        delete process.env.MCP_AUTOMATION_CLIENT_MODE;
        delete process.env.MCP_CONNECTION_TIMEOUT_MS;
        delete process.env.MCP_REQUEST_TIMEOUT_MS;
        delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
        delete process.env.UNREAL_CONNECTION_TIMEOUT;
        vi.resetModules();

        const mod = await import('../../src/config.js');

        expect(mod.config.MCP_ROUTE_STDOUT_LOGS).toBe(true);
        expect(mod.config.MCP_AUTOMATION_PORT).toBe(8091);
        expect(mod.config.MCP_AUTOMATION_CLIENT_MODE).toBe(false);
        expect(mod.config.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
        expect(mod.config.MCP_REQUEST_TIMEOUT_MS).toBe(30000);
    });
});
