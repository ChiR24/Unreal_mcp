import { describe, expect, it } from 'vitest';
import { searchGatewayCapabilities as searchGatewayCatalog } from '../../../src/server/gateway/gateway-search.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

// Progressive, searchable gateway discovery + guided self-correction.
// These cases lock the NEW contract: compact search, paginated/filterable
// tool/action/param drill-down, single-parameter detail, and structured
// nextCall guidance on invalid calls. They fail against the pre-change
// implementation (which dumps inputSchema / full parameter lists).

describe('progressive search stays compact', () => {
  const result = searchGatewayCatalog({ query: 'asset' }) as Record<string, unknown>;
  const results = result.results as Array<Record<string, unknown>>;

  it('returns matches without inputSchema or parameterNames bodies', () => {
    expect(result.success).toBe(true);
    expect(result.operation).toBe('search');
    for (const tool of results) {
      expect(tool.inputSchema).toBeUndefined();
      expect(tool.parameterNames).toBeUndefined();
    }
  });

  it('still keeps enough to route a hit back to its parent tool and action', () => {
    expect(results.some((row) => row.parentTool === 'manage_asset')).toBe(true);
    expect(results.every((row) => typeof row.action === 'string')).toBe(true);
  });
});

describe('describe tool-only returns a summary + paginated actions (no schema dump)', () => {
  const result = describeGatewayCapability({ tool: 'manage_tools' }) as Record<string, unknown>;

  it('omits inputSchema and parameterNames', () => {
    expect(result.success).toBe(true);
    expect(result.inputSchema).toBeUndefined();
    expect(result.parameterNames).toBeUndefined();
  });

  it('returns a paginated/filterable action list with metadata', () => {
    expect(result.operation).toBe('describe');
    expect(result.tool).toBe('manage_tools');
    expect(result.perActionSchemas).toBe(false);
    expect(Array.isArray(result.actions)).toBe(true);
    expect(result.actionCount).toBeGreaterThan(0);
    expect(typeof result.actionLimit).toBe('number');
    expect(typeof result.actionHasMore).toBe('boolean');
    expect(isRecord(result.drillDown)).toBe(true);
  });

  it('filters and paginates the action list via query/limit/offset', () => {
    const total = describeGatewayCapability({ tool: 'manage_tools' }) as Record<string, unknown>;
    const paged = describeGatewayCapability({
      tool: 'manage_tools',
      query: 'category',
      limit: 2,
      offset: 0
    }) as Record<string, unknown>;
    const pagedActions = paged.actions as string[];
    expect(pagedActions.length).toBeLessThanOrEqual(2);
    expect(pagedActions.every((a) => a.includes('category'))).toBe(true);
    expect((total.actionCount as number) >= (pagedActions.length)).toBe(true);
  });
});

describe('guided self-correction on invalid discovery calls', () => {
  it('unknown tool returns closest-match suggestions and a callable nextCall', () => {
    const result = describeGatewayCapability({ tool: 'manage_asts' }) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect((result.suggestions as string[]).includes('manage_asset')).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(typeof next.tool).toBe('string');
  });

  it('unknown action returns suggestions and a nextCall drilling into a valid action', () => {
    const result = describeGatewayCapability({ tool: 'manage_tools', action: 'get_stat' }) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(Array.isArray(result.availableActions)).toBe(true);
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect((result.suggestions as string[]).includes('get_status')).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_tools');
    expect(next.action).toBe('get_status');
  });
});
