import { describe, expect, it } from 'vitest';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import {
    describeGatewayCapability,
    handleUnrealGatewayCall,
    searchGatewayCatalog
} from '../../../src/server/tool-registry-gateway.js';
import { buildGatewayToolDefinition } from '../../../src/server/tool-registry-listing.js';

const logger = new Logger('unreal-gateway-test', 'error');

function makeContext(): GatewayContext {
    const tools: ITools = {
        systemTools: {
            executeConsoleCommand: async () => ({ success: false }),
            getProjectSettings: async () => ({})
        },
        assetResources: { list: async () => ({}) }
    };
    return {
        tools,
        logger,
        elicitationTimeoutMs: 1000,
        ensureConnected: async () => false
    };
}

describe('unreal gateway public list', () => {
    it('advertises exactly one stable tool named unreal', () => {
        const tool = buildGatewayToolDefinition();
        expect(tool.name).toBe('unreal');
        expect(tool.inputSchema).toBeDefined();
        expect(tool.outputSchema).toBeDefined();
    });

    it('reports perActionSchemas false in describe output', () => {
        const result = describeGatewayCapability({ tool: 'manage_tools' }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.perActionSchemas).toBe(false);
    });
});

// Task 24: search ranks canonical capability records, so a result row is a
// capability (`capability`/`parentTool`/`action`), not a parent tool (`name`).
describe('unreal gateway search', () => {
    it('browses the whole canonical catalog when the query is empty', () => {
        const result = searchGatewayCatalog({ limit: 25 }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.operation).toBe('search');
        const results = result.results as Array<Record<string, unknown>>;
        expect(results.length).toBe(25);
        expect(result.total).toBe(1401);
        expect(result.hasMore).toBe(true);
    });

    it('ranks capabilities of the matching parent for a keyword query', () => {
        const result = searchGatewayCatalog({ query: 'asset' }) as Record<string, unknown>;
        const results = result.results as Array<Record<string, unknown>>;
        expect(results.length).toBeGreaterThan(0);
        expect(results.length).toBeLessThanOrEqual(25);
        expect(results.some((row) => row.parentTool === 'manage_asset')).toBe(true);
        expect(results.every((row) => typeof row.capability === 'string')).toBe(true);
    });

    it('bounds pagination and reports hasMore correctly', () => {
        const first = searchGatewayCatalog({ tool: 'manage_tools', limit: 5, offset: 0 }) as Record<string, unknown>;
        expect((first.results as Array<unknown>).length).toBe(5);
        expect(first.hasMore).toBe(true);

        const total = first.total as number;
        const last = searchGatewayCatalog({ tool: 'manage_tools', limit: 25, offset: 5 }) as Record<string, unknown>;
        expect((last.results as Array<unknown>).length).toBe(total - 5);
        expect(last.hasMore).toBe(false);
    });

    it('caps the limit at the maximum and ignores negative offsets', () => {
        const result = searchGatewayCatalog({ limit: 9999, offset: -5 }) as Record<string, unknown>;
        expect((result.results as Array<unknown>).length).toBeLessThanOrEqual(25);
        expect(result.offset).toBe(0);
    });
});

describe('unreal gateway describe', () => {
    it('returns an exact tool contract with exact action casing', () => {
        const result = describeGatewayCapability({ tool: 'manage_tools' }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.tool).toBe('manage_tools');
        const actions = result.actions as string[];
        expect(actions).toContain('get_status');
        expect(actions).toContain('disable_category');
    });

    it('narrows a legacy tool+action pair to that action\'s exact capability', () => {
        const result = describeGatewayCapability({ tool: 'manage_tools', action: 'get_status' }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.action).toBe('get_status');
        expect(result.scope).toBe('capability');
        expect(result.capability).toBe('manage_tools.get_status');
        expect(result.migratedFrom).toEqual({ tool: 'manage_tools', action: 'get_status' });
    });

    it('rejects an unknown tool', () => {
        const result = describeGatewayCapability({ tool: 'does_not_exist' }) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_TOOL');
    });

    it('rejects an unknown action with available actions', () => {
        const result = describeGatewayCapability({ tool: 'manage_tools', action: 'nope' }) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_ACTION');
        expect(result.availableActions).toBeDefined();
    });
});

describe('unreal gateway execute validation', () => {
    const context = makeContext();

    it('rejects an unknown target tool before connecting', async () => {
        const result = await handleUnrealGatewayCall({ operation: 'execute', tool: 'nope', action: 'x' }, context) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_TOOL');
    });

    it('rejects an unknown target action before connecting', async () => {
        const result = await handleUnrealGatewayCall({ operation: 'execute', tool: 'manage_tools', action: 'nope' }, context) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_ACTION');
    });

    it('rejects params that override action', async () => {
        const result = await handleUnrealGatewayCall(
            { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { action: 'hack' } },
            context
        ) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('INVALID_PARAMS');
    });

    it('rejects undeclared parameter keys', async () => {
        const result = await handleUnrealGatewayCall(
            { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { bogus: 1 } },
            context
        ) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    });

    it('requires the action at the gateway level, not buried in params', async () => {
        const result = await handleUnrealGatewayCall(
            { operation: 'execute', tool: 'manage_tools', params: { action: 'get_status' } },
            context
        ) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_ACTION');
    });

    it('rejects an execute call that omits the action entirely', async () => {
        const result = await handleUnrealGatewayCall(
            { operation: 'execute', tool: 'manage_tools' },
            context
        ) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_ACTION');
    });
});

describe('unreal gateway configure', () => {
    const context = makeContext();

    it('delegates manage_tools state behavior without changing the public list', async () => {
        const result = await handleUnrealGatewayCall({ operation: 'configure', action: 'get_status' }, context) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.operation).toBe('configure');
        const inner = result.result as { totalTools?: number };
        expect(inner.totalTools).toBe(23);
    });

    it('requires a manage_tools action', async () => {
        const result = await handleUnrealGatewayCall({ operation: 'configure' }, context) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('MISSING_ACTION');
    });
});

describe('unreal gateway operation dispatch', () => {
    const context = makeContext();

    it('rejects an unknown operation', async () => {
        const result = await handleUnrealGatewayCall({ operation: 'frobnicate' }, context) as Record<string, unknown>;
        expect(result.success).toBe(false);
        expect(result.errorCode).toBe('UNKNOWN_OPERATION');
    });
});
