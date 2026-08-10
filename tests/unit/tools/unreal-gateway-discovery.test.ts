import { afterEach, describe, expect, it } from 'vitest';
import { describeGatewayCapability, searchGatewayCatalog, handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { firstAction } from './support/action-fixtures.js';

function makeExecuteContext(): { tools: ITools; logger: Logger; elicitationTimeoutMs: number; ensureConnected: () => Promise<boolean> } {
  const tools = {
    systemTools: {
      executeConsoleCommand: async () => ({}) as never,
      getProjectSettings: async () => ({}) as never
    },
    assetResources: {}
  } as unknown as ITools;
  return {
    tools,
    logger: new Logger('test-gateway-execute'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

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

// Task 24 replaced the tool-union parameter catalog with the action's exact
// schema: describing manage_asset:import used to list 161 union parameters and
// now lists the 4 that `asset.import` actually declares.
describe('describe tool+action returns that action\'s exact contract', () => {
  const result = describeGatewayCapability({ tool: 'manage_asset', action: 'import' }) as Record<string, unknown>;

  it('returns the action-exact inputSchema instead of a union catalog', () => {
    expect(result.success).toBe(true);
    expect(result.scope).toBe('capability');
    expect(result.perActionSchemas).toBe(true);
    expect(isRecord(result.inputSchema)).toBe(true);
    const properties = (result.inputSchema as Record<string, unknown>).properties as Record<string, unknown>;
    expect(Object.keys(properties).sort()).toEqual(['destinationPath', 'overwrite', 'save', 'sourcePath']);
  });

  it('returns a compact parameter list holding only the declared parameters', () => {
    expect(result.action).toBe('import');
    const params = result.parameters as Array<Record<string, unknown>>;
    expect(params).toHaveLength(4);
    expect(result.parameterCount).toBe(4);
    for (const param of params) {
      expect(typeof param.name).toBe('string');
      expect(typeof param.type).toBe('string');
      expect(typeof param.required).toBe('boolean');
      expect(param).not.toHaveProperty('properties');
    }
  });

  it('routes the legacy pair to its canonical capability id', () => {
    expect(result.capability).toBe('asset.import');
    expect(result.migratedFrom).toEqual({ tool: 'manage_asset', action: 'import' });
  });
});

describe('describe tool+action+param returns exactly one parameter schema', () => {
  const result = describeGatewayCapability({
    tool: 'manage_asset',
    action: 'import',
    param: 'sourcePath'
  }) as Record<string, unknown>;

  it('returns a single parameter full schema scoped to the action', () => {
    expect(result.success).toBe(true);
    expect(result.operation).toBe('describe');
    expect(result.parentTool).toBe('manage_asset');
    expect(result.action).toBe('import');
    expect(result.param).toBe('sourcePath');
    expect(result.scope).toBe('parameter');
    expect(result.perActionSchemas).toBe(true);
    expect(result.required).toBe(true);
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
    expect(next.capability).toBe('asset.import');
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

describe('execute gateway errors carry guided self-correction', () => {
  const context = makeExecuteContext();

  it('UNKNOWN_TOOL returns closest-match suggestions and a callable nextCall', async () => {
    const result = (await handleUnrealGatewayCall({ operation: 'execute', tool: 'manage_asts' }, context)) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect((result.suggestions as string[]).includes('manage_asset')).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(typeof next.tool).toBe('string');
  });

  it('UNKNOWN_ACTION returns suggestions and a nextCall drilling into a valid action', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_stat' },
      context
    )) as Record<string, unknown>;
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

  // Task 26 supersession: suggestions are the action's exact declared parameters
  // rather than the parent-tool union, so this case moved off
  // manage_tools.get_status (which declares none) onto a capability that has
  // them. The non-empty suggestion assertion is retained unchanged.
  it('INVALID_PARAMS (non-object params) returns suggestions and a describe nextCall', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asset', action: 'import', params: 'not-an-object' },
      context
    )) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect((result.suggestions as string[]).length).toBeGreaterThan(0);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_asset');
  });
});

describe('execute guided errors cover the remaining named branches', () => {
  const context = makeExecuteContext();

  afterEach(() => {
    dynamicToolManager.reset();
  });

  it('TOOL_DISABLED returns tool + suggestions + a configure nextCall', async () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asset', action: firstAction('manage_asset') },
      context
    )) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('TOOL_DISABLED');
    expect(result.tool).toBe('manage_asset');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('configure');
    expect(next.tool).toBe('manage_asset');
  });

  it('UNDECLARED_PARAMETER returns allowedParameters + suggestions + a describe nextCall', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { bogus: 1 } },
      context
    )) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(Array.isArray(result.allowedParameters)).toBe(true);
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_tools');
    expect(next.action).toBe('get_status');
  });

  it('INVALID_PARAMS (action override) returns no suggestions/nextCall', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { action: 'hack' } },
      context
    )) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.suggestions).toBeUndefined();
    expect(result.nextCall).toBeUndefined();
  });

  it('NOT_CONNECTED (TS-local) returns a search nextCall', async () => {
    const disconnected = {
      tools: context.tools,
      logger: context.logger,
      elicitationTimeoutMs: context.elicitationTimeoutMs,
      ensureConnected: async () => false
    };
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: {} },
      disconnected
    )) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(isRecord(result.nextCall)).toBe(true);
    expect((result.nextCall as Record<string, unknown>).operation).toBe('search');
  });
});
