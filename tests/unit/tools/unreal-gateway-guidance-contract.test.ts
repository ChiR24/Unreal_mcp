import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import Ajv from 'ajv';
import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { HONORED_EXECUTION_OPTION_KEYS } from '../../../src/server/gateway/gateway-option-validate.js';
import {
  UNREAL_GATEWAY_DESCRIPTION,
  UNREAL_GATEWAY_INSTRUCTIONS,
  unrealGatewayToolDefinition
} from '../../../src/tools/catalog/unreal-gateway-definition.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

// The guidance a model reads BEFORE its first call is the cheapest place to
// prevent a wrong call: the tool description (every listing), the initialize
// `instructions` (once per session), and the two responses that used to leave a
// caller with nothing to copy (an empty search page, a bare action-name list).
// Both transports must say the same thing, so the native mirrors are diffed here.

const pluginRoot = resolve(process.cwd(), 'plugins/McpAutomationBridge');
const nativeDefinition = readFileSync(
  resolve(pluginRoot, 'Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayDefinition.cpp'),
  'utf8'
);
const serverInfo = JSON.parse(readFileSync(resolve(pluginRoot, 'Resources/MCP/server-info.json'), 'utf8')) as {
  instructions?: string;
};

/** Join the adjacent TEXT("...") pieces of one SetStringField call into the string the compiler forms. */
function nativeDescription(): string {
  const call = nativeDefinition.match(/SetStringField\(TEXT\("description"\),\s*TEXT\(((?:"(?:[^"\\]|\\.)*"\s*)+)\)\)/);
  expect(call, 'native definition must set a description from adjacent TEXT("...") literals').not.toBeNull();
  return [...(call?.[1] ?? '').matchAll(/"((?:[^"\\]|\\.)*)"/g)]
    .map((piece) => piece[1].replace(/\\"/g, '"').replace(/\\\\/g, '\\'))
    .join('');
}

function validateGatewayArgs(args: unknown): boolean {
  const ajv = new Ajv({ strict: false, allErrors: true });
  return ajv.compile(unrealGatewayToolDefinition.inputSchema)(args) === true;
}

function context(): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => ({ success: true }),
      isCapabilityTokenConfigured: async () => false,
      getAuthority: () => ({ scopes: ['admin'] })
    },
    'guidance-contract'
  );
}

describe('the `unreal` tool description is one procedure, mirrored on both transports', () => {
  it('native and TypeScript publish byte-identical descriptions', () => {
    expect(nativeDescription()).toBe(UNREAL_GATEWAY_DESCRIPTION);
    expect(unrealGatewayToolDefinition.description).toBe(UNREAL_GATEWAY_DESCRIPTION);
  });

  it('states the three steps in order and the copy-the-nextCall rule', () => {
    const search = UNREAL_GATEWAY_DESCRIPTION.indexOf('(1) search');
    const describeStep = UNREAL_GATEWAY_DESCRIPTION.indexOf('(2) describe');
    const execute = UNREAL_GATEWAY_DESCRIPTION.indexOf('(3) execute');
    expect(search).toBeGreaterThanOrEqual(0);
    expect(describeStep).toBeGreaterThan(search);
    expect(execute).toBeGreaterThan(describeStep);
    expect(UNREAL_GATEWAY_DESCRIPTION).toContain('nextCall');
    expect(UNREAL_GATEWAY_DESCRIPTION).toContain('never put action or subAction inside params');
  });

  it('the native definition mirrors the TypeScript param and query wording', () => {
    const props = unrealGatewayToolDefinition.inputSchema.properties as Record<string, { description?: string }>;
    for (const name of ['query', 'tool', 'action', 'param', 'params']) {
      const piece = nativeDefinition.match(new RegExp(`\\.(?:String|Object)\\(TEXT\\("${name}"\\),\\s*TEXT\\("([^"]*)"\\)`));
      expect(piece, `native definition must declare ${name}`).not.toBeNull();
      expect(piece?.[1], `${name} description diverged between surfaces`).toBe(props[name]?.description);
    }
  });
});

describe('initialize `instructions` carry the same procedure on both transports', () => {
  it('the native server-info.json text equals the TypeScript constant', () => {
    expect(serverInfo.instructions).toBe(UNREAL_GATEWAY_INSTRUCTIONS);
  });

  it('covers workflow, recovery and conventions without naming a transport-only selector', () => {
    for (const phrase of ['1. search', '2. describe', '3. execute', 'Recovery', 'UNKNOWN_', 'INVALID_PARAMS', 'NOT_CONNECTED', '/Game']) {
      expect(UNREAL_GATEWAY_INSTRUCTIONS).toContain(phrase);
    }
    // `capability` is a TypeScript-only selector; the shared text must steer a
    // native client through nextCall, which resolves on both surfaces.
    expect(UNREAL_GATEWAY_INSTRUCTIONS).not.toMatch(/"capability"\s*:/);
  });
});

describe('`options` has a legal input channel on the advertised tool', () => {
  it('declares exactly the honored execution option keys, closed to anything else', () => {
    const options = (unrealGatewayToolDefinition.inputSchema.properties as Record<string, { properties?: object; additionalProperties?: boolean }>).options;
    expect(Object.keys(options?.properties ?? {}).sort()).toEqual([...HONORED_EXECUTION_OPTION_KEYS].sort());
    expect(options?.additionalProperties).toBe(false);
  });

  it('accepts an execute call carrying honored options and rejects an unsupported one', () => {
    const base = { operation: 'execute', tool: 'control_actor', action: 'spawn', params: { classPath: '/Script/Engine.StaticMeshActor' } };
    expect(validateGatewayArgs({ ...base, options: { idempotencyKey: 'spawn-1', timeoutMs: 5000 } })).toBe(true);
    expect(validateGatewayArgs({ ...base, options: { savePolicy: 'none' } })).toBe(false);
  });

  it('the native /mcp gateway declares the same options sibling', () => {
    expect(nativeDefinition.includes('.Object(TEXT("options")')).toBe(true);
  });
});

describe('responses that used to leave nothing to copy now carry a next step', () => {
  it('an empty search page explains what to change and hands over an executable describe', async () => {
    const empty = await handleUnrealGatewayCall({ operation: 'search', query: 'zzzqqq xxyyzz' }, context());
    expect(empty.total).toBe(0);
    expect(String(empty.message)).toContain('describe');
    expect(empty.nextCall).toEqual({ operation: 'describe' });
    const browse = await handleUnrealGatewayCall(empty.nextCall as Record<string, unknown>, context());
    expect(browse.success).toBe(true);
    expect(browse.scope).toBe('catalog');
  });

  it('a tool-only describe points at a filtered search whose rows carry summaries', async () => {
    const summary = await handleUnrealGatewayCall({ operation: 'describe', tool: 'manage_blueprint' }, context());
    expect(summary.browse).toEqual({ operation: 'search', tool: 'manage_blueprint' });
    const rows = await handleUnrealGatewayCall({ ...(summary.browse as Record<string, unknown>), query: 'add variable' }, context());
    const first = (rows.results as Array<Record<string, unknown>>)[0];
    expect(first?.capability).toBe('blueprint.add_variable');
    expect(typeof first?.summary).toBe('string');
  });
});
