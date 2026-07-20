import { afterAll, afterEach, beforeAll, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { consolidatedToolDefinitions } from '../../../src/tools/catalog/consolidated-tool-definitions.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Mock the consolidated tool handler so execute can reach the RESULT_TOO_LARGE gate
// without a live Unreal connection or a real dispatch.
vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => ({ success: true, data: { big: 'x'.repeat(200_000) } }))
}));

const NATIVE_CATALOG_PATH = path.resolve(
  __dirname,
  '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayCatalog.cpp'
);
const NATIVE_CATALOG_HEADER_PATH = path.resolve(
  __dirname,
  '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayCatalog.h'
);
const NATIVE_GUIDANCE_PATH = path.resolve(
  __dirname,
  '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayGuidance.cpp'
);
const NATIVE_GUIDANCE_HEADER_PATH = path.resolve(
  __dirname,
  '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayGuidance.h'
);
// Task 27 split the single pre-split validation file into a staged pipeline
// (parse/resolve -> orchestrate -> schema). Guided-error parity is a property of
// the pipeline, so it is asserted over the modules that together implement it.
const NATIVE_EXECUTE_PIPELINE = [
  'McpNativeGatewayExecuteRequest.cpp',
  'McpNativeGatewayValidation.cpp',
  'McpNativeGatewaySchemaValidation.cpp',
]
  .map((module) =>
    readFileSync(
      path.resolve(
        __dirname,
        `../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute/${module}`,
      ),
      'utf8',
    ),
  )
  .join('\n');
const NATIVE_CATALOG = readFileSync(NATIVE_CATALOG_PATH, 'utf8');
const NATIVE_CATALOG_HEADER = readFileSync(NATIVE_CATALOG_HEADER_PATH, 'utf8');
const NATIVE_GUIDANCE = readFileSync(NATIVE_GUIDANCE_PATH, 'utf8');
const NATIVE_GUIDANCE_HEADER = readFileSync(NATIVE_GUIDANCE_HEADER_PATH, 'utf8');

function makeContext(ensureConnected: () => Promise<boolean> = async () => true): GatewayContext {
  const tools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  } as unknown as ITools;
  return {
    tools,
    logger: new Logger('parity-execute', 'error'),
    elicitationTimeoutMs: 1000,
    ensureConnected
  };
}

function firstAction(toolName: string): string {
  const def = consolidatedToolDefinitions.find((tool) => tool.name === toolName);
  const props = isRecord(def?.inputSchema) && isRecord(def.inputSchema.properties) ? def.inputSchema.properties : undefined;
  const action = isRecord(props) ? props.action : undefined;
  const enumArr = isRecord(action) && Array.isArray(action.enum) ? action.enum : [];
  const first = enumArr.find((value) => typeof value === 'string');
  return typeof first === 'string' ? first : 'x';
}

afterEach(() => {
  dynamicToolManager.reset();
});

describe('TS guided execute-error parity: deterministic suggestions + executable nextCall', () => {
  it('UNKNOWN_TOOL gives deterministic suggestions and a describe nextCall', async () => {
    const payload = { operation: 'execute', tool: 'manage_asts' };
    const a = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    const b = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    expect(a.success).toBe(false);
    expect(a.errorCode).toBe('UNKNOWN_TOOL');
    expect(Array.isArray(a.suggestions)).toBe(true);
    expect((a.suggestions as string[])[0]).toBe('manage_asset');
    expect(a.nextCall).toEqual(b.nextCall);
    expect(isRecord(a.nextCall)).toBe(true);
    const next = a.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_asset');
  });

  it('UNKNOWN_ACTION gives deterministic suggestions drilling into a valid action', async () => {
    const payload = { operation: 'execute', tool: 'manage_tools', action: 'get_stat' };
    const a = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    const b = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    expect(a.errorCode).toBe('UNKNOWN_ACTION');
    expect(Array.isArray(a.availableActions)).toBe(true);
    expect((a.suggestions as string[])[0]).toBe('get_status');
    expect(a.nextCall).toEqual(b.nextCall);
    const next = a.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_tools');
    expect(next.action).toBe('get_status');
  });

  it('TOOL_DISABLED gives tool + suggestions + a configure nextCall', async () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const payload = { operation: 'execute', tool: 'manage_asset', action: firstAction('manage_asset') };
    const a = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    const b = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    expect(a.errorCode).toBe('TOOL_DISABLED');
    expect(a.tool).toBe('manage_asset');
    expect(Array.isArray(a.suggestions)).toBe(true);
    expect(a.nextCall).toEqual(b.nextCall);
    const next = a.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('configure');
    expect(next.tool).toBe('manage_asset');
  });

  it('INVALID_PARAMS (action override) gives NO suggestions/nextCall', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { action: 'hack' } },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.suggestions).toBeUndefined();
    expect(result.nextCall).toBeUndefined();
  });

  it('INVALID_PARAMS (non-object) gives tool+action + first-3 suggestions + describe nextCall', async () => {
    const payload = { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: 'not-an-object' };
    const a = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    const b = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    expect(a.errorCode).toBe('INVALID_PARAMS');
    expect(a.tool).toBe('manage_tools');
    expect(a.action).toBe('get_status');
    expect(Array.isArray(a.suggestions)).toBe(true);
    expect((a.suggestions as string[]).length).toBeLessThanOrEqual(3);
    expect(a.nextCall).toEqual(b.nextCall);
    const next = a.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_tools');
    expect(next.action).toBe('get_status');
  });

  // Task 26 supersession: suggestions now come from the action's exact declared
  // parameters instead of the parent-tool union, so this case moved off
  // manage_tools.get_status (which declares none) onto a capability that has
  // them. Every assertion, including the non-empty suggestion list, is unchanged.
  it('UNDECLARED_PARAMETER gives allowedParameters + suggestions + describe nextCall drilling to param', async () => {
    const payload = {
      operation: 'execute',
      tool: 'manage_asset',
      action: 'import',
      params: { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A', bogus: 1 }
    };
    const a = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    const b = (await handleUnrealGatewayCall(payload, makeContext())) as Record<string, unknown>;
    expect(a.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(Array.isArray(a.allowedParameters)).toBe(true);
    expect(Array.isArray(a.suggestions)).toBe(true);
    expect((a.suggestions as string[]).length).toBeGreaterThan(0);
    expect(a.nextCall).toEqual(b.nextCall);
    const next = a.nextCall as Record<string, unknown>;
    expect(next.operation).toBe('describe');
    expect(next.tool).toBe('manage_asset');
    expect(next.action).toBe('import');
    expect(typeof next.param).toBe('string');
  });

  it('NOT_CONNECTED (TS-local) gives a search nextCall', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: {} },
      makeContext(async () => false)
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(isRecord(result.nextCall)).toBe(true);
    expect((result.nextCall as Record<string, unknown>).operation).toBe('search');
  });

  it('RESULT_TOO_LARGE (TS-local) reports resultChars', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: {} },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
    expect((result.resultChars as number) > 100_000).toBe(true);
  });
});

describe('native execute pipeline emits the same guided-error contract', () => {
  it('routes every guided branch to an executable recovery call', () => {
    // Each guided failure must hand back a call the client can run verbatim:
    // unknown capability/tool -> search, unknown action or bad param -> describe,
    // disabled capability -> configure.
    for (const operation of ['search', 'describe', 'configure']) {
      expect(
        NATIVE_EXECUTE_PIPELINE,
        `guided errors must offer a '${operation}' recovery call`,
      ).toContain(`GatewayBuildNextCall(TEXT("${operation}")`);
    }
    expect(NATIVE_EXECUTE_PIPELINE).toContain('SetObjectField(TEXT("nextCall")');
    expect(NATIVE_EXECUTE_PIPELINE).toContain('SetArrayField(TEXT("suggestions")');
  });

  it('bounds closest-match suggestions to the shared limit of 3', () => {
    expect(NATIVE_EXECUTE_PIPELINE).toContain('GatewayClosestMatches(');
    for (const [, limit] of NATIVE_EXECUTE_PIPELINE.matchAll(
      /GatewayClosestMatches\([^;]*?,\s*(\d+)\)/gu,
    )) {
      expect(limit).toBe('3');
    }
  });

  it('carries the same errorCode literals as the TS gateway', () => {
    for (const code of ['UNKNOWN_TOOL', 'UNKNOWN_ACTION', 'TOOL_DISABLED', 'INVALID_PARAMS', 'UNDECLARED_PARAMETER']) {
      expect(NATIVE_EXECUTE_PIPELINE).toContain(`TEXT("${code}")`);
    }
  });

  it('does NOT surface NOT_CONNECTED / RESULT_TOO_LARGE at gateway validation (TS-local asymmetries)', () => {
    expect(NATIVE_EXECUTE_PIPELINE).not.toContain('NOT_CONNECTED');
    expect(NATIVE_EXECUTE_PIPELINE).not.toContain('RESULT_TOO_LARGE');
  });
});

describe('native GatewayClosestMatches empty-target matches TS first-3 slice', () => {
  it('bounds the empty-target candidate list to the requested limit (parity with TS slice(0, limit))', () => {
    expect(NATIVE_GUIDANCE).toContain('FMath::Min(Candidates.Num(), Limit)');
  });

  it('declares the shared 3-suggestion default on the guidance seam', () => {
    expect(NATIVE_GUIDANCE_HEADER).toContain(
      'TArray<FString> GatewayClosestMatches(const FString& Target, const TArray<FString>& Candidates, int32 Limit = 3);'
    );
  });

  it('keeps one closest-match implementation, reachable from the catalog header', () => {
    expect(NATIVE_CATALOG_HEADER).toContain('#include "MCP/Gateway/McpNativeGatewayGuidance.h"');
    expect(NATIVE_CATALOG).not.toContain('GatewayClosestMatches(');
  });
});

describe('adversarial guided-execute-error probes', () => {
  it('malformed: null tool -> UNKNOWN_TOOL with guidance (not a crash)', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: null, action: 'x' },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
  });

  it('malformed: numeric action -> UNKNOWN_ACTION with guidance', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 7 as unknown as string },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(Array.isArray(result.suggestions)).toBe(true);
  });

  it('malformed: numeric params -> INVALID_PARAMS (non-object) with guidance', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: 42 as unknown as Record<string, unknown> },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
  });

  it('dirty: valid params mixed with an unknown key -> UNDECLARED_PARAMETER', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { bogus: 1, alsoBad: 2 } },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(Array.isArray(result.allowedParameters)).toBe(true);
    expect(Array.isArray(result.suggestions)).toBe(true);
  });

  it('dirty: params carrying action/subAction -> INVALID_PARAMS (override), no guidance', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { subAction: 'hack' } },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.suggestions).toBeUndefined();
    expect(result.nextCall).toBeUndefined();
  });

  it('stale: pre-rename tool name resolves to the closest current tool', async () => {
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asts' },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect((result.suggestions as string[])[0]).toBe('manage_asset');
  });

  it('flaky: connection failure is deterministic -> NOT_CONNECTED every attempt', async () => {
    let attempts = 0;
    const result = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: {} },
      makeContext(async () => {
        attempts += 1;
        return false;
      })
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(attempts).toBe(1);
    expect((result.nextCall as Record<string, unknown>).operation).toBe('search');
  });

  it('misleading: near-miss param resolves deterministically to a valid parameter', async () => {
    const result = (await handleUnrealGatewayCall(
      // The declared required parameters are supplied so the near-miss key is the
      // only violation left: all three surfaces check `required` before
      // `additionalProperties`, so omitting them would report the missing
      // parameter first and never exercise the near-miss suggestion.
      {
        operation: 'execute',
        tool: 'manage_asset',
        action: 'import',
        params: { sourcePath: '/x', destinationPath: '/Game/X', sorcePath: '/x' }
      },
      makeContext()
    )) as Record<string, unknown>;
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    const allowed = result.allowedParameters as string[];
    const suggestions = result.suggestions as string[];
    expect(suggestions.length).toBeGreaterThan(0);
    expect(allowed).toContain(suggestions[0]);
    const next = result.nextCall as Record<string, unknown>;
    expect(next.param).toBe(suggestions[0]);
  });
});

describe('real in-memory MCP gateway probe returns guided execute errors', () => {
  type Created = ReturnType<typeof import('../../../src/server/server-factory.js').createServer>;
  let created: Created | undefined;
  let client: import('@modelcontextprotocol/sdk/client/index.js').Client | undefined;

  beforeAll(async () => {
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    process.env.NODE_ENV = 'test';
    const mod = await import('../../../src/server/server-factory.js');
    created = mod.createServer();
    const { Client } = await import('@modelcontextprotocol/sdk/client/index.js');
    const { InMemoryTransport } = await import('@modelcontextprotocol/sdk/inMemory.js');
    client = new Client({ name: 'parity-probe', version: '1.0.0' }, { capabilities: {} });
    const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
    await created.server.connect(serverTransport);
    await client.connect(clientTransport, { timeout: 15000 });
  }, 40000);

  afterAll(async () => {
    try {
      await client?.transport?.close();
    } catch {
      /* ignore */
    }
    try {
      created?.automationBridge.stop();
    } catch {
      /* ignore */
    }
    try {
      created?.bridge.dispose();
    } catch {
      /* ignore */
    }
    try {
      created?.metricsServer?.close();
    } catch {
      /* ignore */
    }
  });

  function getStructured(content: unknown): Record<string, unknown> {
    const r = content as { structuredContent?: unknown; content?: unknown };
    if (r.structuredContent && typeof r.structuredContent === 'object') {
      return r.structuredContent as Record<string, unknown>;
    }
    if (Array.isArray(r.content)) {
      const first = r.content[0] as { type?: string; text?: string } | undefined;
      if (first && typeof first.text === 'string') {
        try {
          return JSON.parse(first.text) as Record<string, unknown>;
        } catch {
          /* ignore */
        }
      }
    }
    return {};
  }

  it('unknown tool execute returns suggestions + nextCall through the real transport', async () => {
    const c = client;
    if (!c) throw new Error('probe client not ready');
    const res = await c.callTool(
      { name: 'unreal', arguments: { operation: 'execute', tool: 'manage_asts' } },
      undefined,
      { timeout: 15000 }
    );
    const sc = getStructured(res);
    expect(sc.success).toBe(false);
    expect(sc.errorCode).toBe('UNKNOWN_TOOL');
    expect(Array.isArray(sc.suggestions)).toBe(true);
    expect((sc.suggestions as string[])[0]).toBe('manage_asset');
    expect(isRecord(sc.nextCall)).toBe(true);
    expect((sc.nextCall as Record<string, unknown>).operation).toBe('describe');
  }, 25000);

  it('undeclared parameter execute returns allowedParameters + nextCall drilling to param', async () => {
    const c = client;
    if (!c) throw new Error('probe client not ready');
    const res = await c.callTool(
      { name: 'unreal', arguments: { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { bogus: 1 } } },
      undefined,
      { timeout: 15000 }
    );
    const sc = getStructured(res);
    expect(sc.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(Array.isArray(sc.allowedParameters)).toBe(true);
    expect(isRecord(sc.nextCall)).toBe(true);
    expect((sc.nextCall as Record<string, unknown>).operation).toBe('describe');
  }, 25000);
});
