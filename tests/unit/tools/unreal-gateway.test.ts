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

describe('unreal gateway search', () => {
    it('returns all canonical tools when the query is empty', () => {
        const result = searchGatewayCatalog({ limit: 25 }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.operation).toBe('search');
        const results = result.results as Array<Record<string, unknown>>;
        expect(results.length).toBe(23);
        expect(result.total).toBe(23);
        expect(result.hasMore).toBe(false);
    });

    it('filters by query across name, description, and actions', () => {
        const result = searchGatewayCatalog({ query: 'asset' }) as Record<string, unknown>;
        const results = result.results as Array<Record<string, unknown>>;
        expect(results.length).toBeGreaterThan(0);
        expect(results.length).toBeLessThanOrEqual(25);
        expect(results.some((tool) => tool.name === 'manage_asset')).toBe(true);
        const matches = results.every((tool) => {
            const haystack = [
                tool.name,
                tool.category ?? '',
                tool.description,
                ...((tool.actions as string[]) ?? [])
            ].join(' ').toLowerCase();
            return haystack.includes('asset');
        });
        expect(matches).toBe(true);
    });

    it('bounds pagination and reports hasMore correctly', () => {
        const first = searchGatewayCatalog({ limit: 5, offset: 0 }) as Record<string, unknown>;
        const firstResults = first.results as Array<unknown>;
        expect(firstResults.length).toBe(5);
        expect(first.hasMore).toBe(true);

        const last = searchGatewayCatalog({ limit: 25, offset: 20 }) as Record<string, unknown>;
        expect((last.results as Array<unknown>).length).toBe(3);
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

    it('narrows to a single exact action when provided', () => {
        const result = describeGatewayCapability({ tool: 'manage_tools', action: 'get_status' }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.action).toBe('get_status');
        expect(result.actions).toEqual(['get_status']);
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
