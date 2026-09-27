import { describe, expect, it } from 'vitest';
import { searchGatewayCapabilities as searchGatewayCatalog } from '../../../src/server/gateway/gateway-search.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { ALL_CAPABILITY_RECORD_COUNT } from '../../../src/tools/catalog/capabilities/records/aggregate.js';

describe('unreal gateway public list', () => {
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
        // Folded families carry longer summaries, so 25 rows no longer fit the
        // default byte budget; 20 do, and the page still reports the full total.
        const result = searchGatewayCatalog({ limit: 20 }) as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(result.operation).toBe('search');
        const results = result.results as Array<Record<string, unknown>>;
        expect(results.length).toBe(20);
        expect(result.total).toBe(ALL_CAPABILITY_RECORD_COUNT);
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
