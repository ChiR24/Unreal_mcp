import { afterEach, describe, expect, it, vi } from 'vitest';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Mock the consolidated tool handler so execute can reach the RESULT_TOO_LARGE gate
// without a live Unreal connection or a real dispatch.
const handleConsolidatedToolCall = vi.fn(async (_tool: string, _payload: Record<string, unknown>): Promise<unknown> => ({ success: true, data: { big: 'x'.repeat(200_000) } }));

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
// The disabled-capability/configure guidance helper lives in MCP/Gateway
// (extracted from Validation.cpp under the 250-pure-line ceiling), so the
// guidance module is part of the pipeline too.
const NATIVE_EXECUTE_PIPELINE = [
  'MCP/Execute/McpNativeGatewayExecuteRequest.cpp',
  'MCP/Execute/McpNativeGatewayValidation.cpp',
  'MCP/Execute/McpNativeGatewaySchemaValidation.cpp',
  'MCP/Gateway/McpNativeGatewayGuidance.cpp',
]
  .map((module) =>
    readFileSync(
      path.resolve(
        __dirname,
        `../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/${module}`,
      ),
      'utf8',
    ),
  )
  .join('\n');
const NATIVE_CATALOG_HEADER = readFileSync(NATIVE_CATALOG_HEADER_PATH, 'utf8');
const NATIVE_GUIDANCE = readFileSync(NATIVE_GUIDANCE_PATH, 'utf8');
const NATIVE_GUIDANCE_HEADER = readFileSync(NATIVE_GUIDANCE_HEADER_PATH, 'utf8');

function makeContext(ensureConnected: () => Promise<boolean> = async () => true): GatewayContext {
  const tools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (tool: string, payload: Record<string, unknown>) => handleConsolidatedToolCall(tool, payload)
    }
  } as unknown as ITools;
  return {
    tools,
    logger: new Logger('parity-execute', 'error'),
    ensureConnected
  };
}

afterEach(() => {
  dynamicToolManager.reset();
});

describe('TS guided execute-error parity: deterministic suggestions + executable nextCall', () => {
  it('UNKNOWN_ACTION sends an action another tool owns to that tool, on execute and describe', async () => {
    const payload = { tool: 'manage_blueprint', action: 'set_blueprint_variables' };
    const owner = { operation: 'describe', tool: 'control_actor', action: 'set_blueprint_variables' };
    const executed = (await handleUnrealGatewayCall({ operation: 'execute', ...payload }, makeContext())) as Record<string, unknown>;
    expect(executed.errorCode).toBe('UNKNOWN_ACTION');
    expect(executed.message).toBe(
      "Unknown action for manage_blueprint. 'set_blueprint_variables' is a control_actor action. Call describe before execute.");
    expect(executed.nextCall).toEqual(owner);
    const described = (await handleUnrealGatewayCall({ operation: 'describe', ...payload }, makeContext())) as Record<string, unknown>;
    expect(described.errorCode).toBe('UNKNOWN_ACTION');
    expect(described.message).toBe("Unknown action 'set_blueprint_variables' for manage_blueprint. 'set_blueprint_variables' is a control_actor action.");
    expect(described.nextCall).toEqual(owner);
  });

  it('UNKNOWN_ACTION searches when no suggestion shares the verb, and tolerates a one-letter verb typo', async () => {
    // save_asset used to be sent to move_asset; saving is control_editor.save_all.
    const save = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asset', action: 'save_asset' }, makeContext())) as Record<string, unknown>;
    expect(save.nextCall).toEqual({ operation: 'search', query: 'save asset' });
    const typo = (await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'gett_status' }, makeContext())) as Record<string, unknown>;
    expect(typo.nextCall).toEqual({ operation: 'describe', tool: 'manage_tools', action: 'get_status' });
  });

  it('native UNKNOWN_ACTION guidance follows the same owner / verb-search / closest order', () => {
    expect(NATIVE_GUIDANCE).toContain('FMcpUnknownActionGuide GatewayGuideUnknownAction(');
    expect(NATIVE_GUIDANCE).toMatch(/Others\.Num\(\) == 1[\s\S]*is a %s action[\s\S]*GatewayLevenshtein\(ActionVerb\(S\), Verb\) <= 1[\s\S]*TEXT\("search"\)[\s\S]*TEXT\("query"\)/u);
    expect(NATIVE_EXECUTE_PIPELINE).toContain('GatewayGuideUnknownAction(');
    expect(NATIVE_EXECUTE_PIPELINE).toContain('GetParentsWithAction(Action)');
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
    expect(NATIVE_CATALOG_HEADER).not.toContain('GatewayClosestMatches(');
  });
});
