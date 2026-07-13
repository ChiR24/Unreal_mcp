import { describe, expect, it } from 'vitest';
import { describeGatewayCapability, searchGatewayCatalog } from '../../../src/server/tool-registry-gateway.js';
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

  it('still keeps enough to filter by action name', () => {
    expect(results.some((tool) => tool.name === 'manage_asset')).toBe(true);
    const everyHasActions = results.every((tool) => Array.isArray(tool.actions));
    expect(everyHasActions).toBe(true);
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

describe('describe tool+action returns a parameter catalog (no inputSchema)', () => {
  const result = describeGatewayCapability({ tool: 'manage_asset', action: 'import' }) as Record<string, unknown>;

  it('does not return the full inputSchema', () => {
    expect(result.success).toBe(true);
    expect(result.inputSchema).toBeUndefined();
  });

  it('returns a paginated/filterable compact parameter catalog labeled as the tool-union', () => {
    expect(result.action).toBe('import');
    expect(result.scope).toBe('union');
    expect(result.perActionSchemas).toBe(false);
    expect(Array.isArray(result.parameters)).toBe(true);
    const params = result.parameters as Array<Record<string, unknown>>;
    if (params.length > 0) {
      const first = params[0];
      expect(typeof first.name).toBe('string');
      expect(typeof first.type).toBe('string');
      // Compact summary only: no nested full schema bodies per entry.
      expect(first).not.toHaveProperty('properties');
    }
    expect(typeof result.parameterCount).toBe('number');
    expect(typeof result.parameterLimit).toBe('number');
    expect(typeof result.parameterHasMore).toBe('boolean');
    expect(isRecord(result.drillDown)).toBe(true);
    const drill = result.drillDown as Record<string, unknown>;
    expect(drill.operation).toBe('describe');
    expect(drill.tool).toBe('manage_asset');
    expect(drill.action).toBe('import');
    expect(typeof drill.param).toBe('string');
  });

  it('filters the parameter catalog by query', () => {
    const filtered = describeGatewayCapability({
      tool: 'manage_asset',
      action: 'import',
      query: 'path'
    }) as Record<string, unknown>;
    const params = filtered.parameters as Array<{ name: string; description?: string }>;
    expect(params.length).toBeGreaterThan(0);
    expect(
      params.every(
        (p) =>
          p.name.toLowerCase().includes('path') ||
          (typeof p.description === 'string' && p.description.toLowerCase().includes('path'))
      )
    ).toBe(true);
  });
});

describe('describe tool+action+param returns exactly one parameter schema', () => {
  const result = describeGatewayCapability({
    tool: 'manage_asset',
    action: 'import',
    param: 'sourcePath'
  }) as Record<string, unknown>;

  it('returns a single parameter full schema with union scope', () => {
    expect(result.success).toBe(true);
    expect(result.operation).toBe('describe');
    expect(result.tool).toBe('manage_asset');
    expect(result.action).toBe('import');
    expect(result.param).toBe('sourcePath');
    expect(result.scope).toBe('union');
    expect(result.perActionSchemas).toBe(false);
    expect(typeof result.required).toBe('boolean');
    expect(isRecord(result.schema)).toBe(true);
    const schema = result.schema as Record<string, unknown>;
    expect(schema.type).toBe('string');
    expect(typeof schema.description).toBe('string');
    // Exactly one parameter: no catalog list is returned.
    expect(result.parameters).toBeUndefined();
  });

  it('rejects an unknown parameter with suggestions and an executable nextCall', () => {
    const bad = describeGatewayCapability({
      tool: 'manage_asset',
      action: 'import',
      param: 'sorcePath'
    }) as Record<string, unknown>;
    expect(bad.success).toBe(false);
    expect(bad.errorCode).toBe('UNKNOWN_PARAM');
    expect(Array.isArray(bad.suggestions)).toBe(true);
    expect((bad.suggestions as string[]).length).toBeGreaterThan(0);
    expect(isRecord(bad.nextCall)).toBe(true);
    const next = bad.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_asset');
    expect(next.action).toBe('import');
    expect(typeof next.param).toBe('string');
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
