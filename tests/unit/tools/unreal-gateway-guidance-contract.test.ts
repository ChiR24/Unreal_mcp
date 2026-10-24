import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import Ajv from 'ajv';
// Ajv's ESM and CJS builds expose the class differently; `new Ajv()` only
// works because the test transform papers over it. Mirrors the interop in
// src/utils/responses/response-validator.ts.
const AjvCtor = (Ajv as typeof Ajv & { default?: typeof Ajv.default }).default ?? Ajv.default;
import { afterEach, describe, expect, it, vi } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { STILL_RUNNING_AFTER_MS } from '../../../src/server/gateway/gateway-execute.js';
import { EXECUTION_OPTION_KEYS } from '../../../src/tools/catalog/capabilities/semantic/execution-options.js';
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

const pluginRoot = resolve(process.cwd(), 'plugins/EngineWire');
const nativeDefinition = readFileSync(
  resolve(pluginRoot, 'Source/EngineWire/Private/MCP/Gateway/McpNativeGatewayDefinition.cpp'),
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

type Json = Record<string, unknown>;
type BuiltSchema = { properties: Json; required: string[]; closed: boolean };

const isPlainObject = (value: unknown): value is Json =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

/**
 * The native `unreal` input schema, rebuilt from the FMcpSchemaBuilder chain in BuildUnrealGatewayToolDefinition.
 * Each method is emulated as McpSchemaBuilder.cpp builds it. A method or argument shape this reader does not know
 * throws, so a new builder call cannot slip past the diff below.
 */
function nativeInputSchema(): Json {
  const body = nativeDefinition.slice(nativeDefinition.indexOf('BuildUnrealGatewayToolDefinition()'));
  let at = body.indexOf('FMcpSchemaBuilder()') + 'FMcpSchemaBuilder()'.length;
  const rest = (): string => body.slice(at);
  const skip = (): void => {
    for (let gap = /^(?:\s+|\/\/[^\n]*)/u.exec(rest()); gap !== null; gap = /^(?:\s+|\/\/[^\n]*)/u.exec(rest())) at += gap[0].length;
  };
  const peek = (token: string): boolean => {
    skip();
    return rest().startsWith(token);
  };
  const take = (token: string): void => {
    if (!peek(token)) throw new Error(`native schema chain: expected '${token}' at '${rest().slice(0, 60)}'`);
    at += token.length;
  };
  const word = (): string => {
    skip();
    const match = /^\w+/u.exec(rest());
    if (match === null) throw new Error(`native schema chain: expected a name at '${rest().slice(0, 60)}'`);
    at += match[0].length;
    return match[0];
  };
  const text = (): string => {
    take('TEXT(');
    let out = '';
    for (let piece = /^\s*"((?:[^"\\]|\\.)*)"/u.exec(rest()); piece !== null; piece = /^\s*"((?:[^"\\]|\\.)*)"/u.exec(rest())) {
      out += (piece[1] ?? '').replace(/\\(.)/gu, '$1');
      at += piece[0].length;
    }
    take(')');
    return out;
  };
  const chain = (): BuiltSchema => {
    const built: BuiltSchema = { properties: {}, required: [], closed: false };
    while (peek('.')) {
      take('.');
      const method = word();
      take('(');
      const args: unknown[] = [];
      while (!peek(')')) {
        if (args.length > 0) take(',');
        args.push(argument());
      }
      take(')');
      if (method === 'Build') return built;
      apply(built, method, args);
    }
    return built;
  };
  const argument = (): unknown => {
    if (peek('TEXT(')) return text();
    if (peek('FString()')) {
      take('FString()');
      return '';
    }
    if (peek('{')) {
      take('{');
      const values = [text()];
      while (peek(',')) {
        take(',');
        values.push(text());
      }
      take('}');
      return values;
    }
    if (peek('[]')) {
      take('[](FMcpSchemaBuilder&');
      const name = word();
      take(')');
      take('{');
      take(name);
      const sub = chain();
      take(';');
      take('}');
      return sub;
    }
    const number = word();
    if (!/^\d+$/u.test(number)) throw new Error(`native schema chain: unexpected argument '${number}'`);
    return Number(number);
  };
  const typed = (type: string, description: unknown): Json =>
    description === '' || description === undefined ? { type } : { type, description };
  const apply = (built: BuiltSchema, method: string, args: unknown[]): void => {
    const name = String(args[0]);
    const arity = `${method}/${args.length}`;
    switch (arity) {
      case 'String/2': built.properties[name] = typed('string', args[1]); return;
      case 'String/4': built.properties[name] = { ...typed('string', args[1]), minLength: args[2], maxLength: args[3] }; return;
      case 'StringEnum/3': built.properties[name] = { ...typed('string', args[2]), enum: args[1] }; return;
      case 'Integer/1': built.properties[name] = typed('integer', undefined); return;
      case 'Integer/2': built.properties[name] = typed('integer', args[1]); return;
      case 'Integer/3': built.properties[name] = { ...typed('integer', args[1]), minimum: args[2] }; return;
      case 'Integer/4': built.properties[name] = { ...typed('integer', args[1]), minimum: args[2], maximum: args[3] }; return;
      case 'ClosedObject/3': {
        const sub = args[2] as BuiltSchema;
        built.properties[name] = {
          ...typed('object', args[1]),
          properties: sub.properties,
          ...(sub.required.length > 0 ? { required: sub.required } : {}),
          additionalProperties: false
        };
        return;
      }
      case 'OpenObject/2': built.properties[name] = { ...typed('object', args[1]), additionalProperties: true }; return;
      case 'Required/1':
        for (const field of args[0] as string[]) if (!built.required.includes(field)) built.required.push(field);
        return;
      case 'Closed/0': built.closed = true; return;
      default: throw new Error(`FMcpSchemaBuilder::${arity} is not read by this test; add it as McpSchemaBuilder.cpp builds it`);
    }
  };
  const built = chain();
  return {
    type: 'object',
    properties: built.properties,
    ...(built.required.length > 0 ? { required: built.required } : {}),
    ...(built.closed ? { additionalProperties: false } : {})
  };
}

/** JSON pointers at which two schemas disagree; arrays compare whole. */
function schemaDifferences(left: unknown, right: unknown, path = ''): string[] {
  if (isPlainObject(left) && isPlainObject(right)) {
    return [...new Set([...Object.keys(left), ...Object.keys(right)])]
      .sort()
      .flatMap((key) => schemaDifferences(left[key], right[key], `${path}/${key}`));
  }
  return JSON.stringify(left) === JSON.stringify(right) ? [] : [path];
}

// The native door mints a single-use nonce with every consent grant (CONSENT_REUSED on replay); the TypeScript door's
// grants carry none. docs/gateway-client-guide.md lists it under "Where the two doors still differ".
const INTENDED_SCHEMA_DIFFERENCES = ['/properties/consent/description', '/properties/consent/properties/nonce'];

function validateGatewayArgs(args: unknown): boolean {
  const ajv = new AjvCtor({ strict: false, allErrors: true });
  return ajv.compile(unrealGatewayToolDefinition.inputSchema)(args) === true;
}

function context(): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => ({ success: true }),
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

});

describe('the `unreal` input schema is the same on both transports', () => {
  it('the native builder chain states every schema fact, with no edits after Build', () => {
    const body = nativeDefinition.slice(nativeDefinition.indexOf('BuildUnrealGatewayToolDefinition()'));
    expect(body).not.toMatch(/InputSchema->|Prop\(/u);
  });

  it('differs from the TypeScript schema only by the native consent nonce', () => {
    expect(schemaDifferences(nativeInputSchema(), unrealGatewayToolDefinition.inputSchema)).toEqual(INTENDED_SCHEMA_DIFFERENCES);
  });
});

describe('initialize `instructions` carry the same procedure on both transports', () => {
  it('the native server-info.json text equals the TypeScript constant', () => {
    expect(serverInfo.instructions).toBe(UNREAL_GATEWAY_INSTRUCTIONS);
  });

  it('covers workflow, recovery and conventions', () => {
    for (const phrase of ['1. search', '2. describe', '3. execute', 'Recovery', 'UNKNOWN_', 'INVALID_PARAMS', 'NOT_CONNECTED', '/Game']) {
      expect(UNREAL_GATEWAY_INSTRUCTIONS).toContain(phrase);
    }
  });
});

describe('`options` has a legal input channel on the advertised tool', () => {
  it('declares exactly the honored execution option keys, closed to anything else', () => {
    const options = (unrealGatewayToolDefinition.inputSchema.properties as Record<string, { properties?: object; additionalProperties?: boolean }>).options;
    expect(Object.keys(options?.properties ?? {}).sort()).toEqual([...EXECUTION_OPTION_KEYS].sort());
    expect(options?.additionalProperties).toBe(false);
  });

  it('accepts an execute call carrying honored options and rejects an unsupported one', () => {
    const base = { operation: 'execute', tool: 'control_actor', action: 'spawn', params: { classPath: '/Script/Engine.StaticMeshActor' } };
    expect(validateGatewayArgs({ ...base, options: { idempotencyKey: 'spawn-1', timeoutMs: 5000 } })).toBe(true);
    expect(validateGatewayArgs({ ...base, options: { savePolicy: 'none' } })).toBe(false);
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
    expect(first?.capability).toBe('blueprint.edit_variable');
    expect(typeof first?.summary).toBe('string');
  });
});

describe('a plugin from another release is named on every failed execute', () => {
  const spawn = { operation: 'execute', tool: 'control_actor', action: 'spawn', params: { classPath: '/Script/Engine.StaticMeshActor' } };
  const mismatched = (reply: Record<string, unknown>): GatewayContext => gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => reply,
      getAuthority: () => ({ scopes: ['admin'] }),
      getVersionMismatch: () => 'Version mismatch: test'
    },
    'version-mismatch'
  );

  it('a failed dispatch carries versionMismatch', async () => {
    const reply = await handleUnrealGatewayCall(spawn, mismatched({ success: false, error: 'UNKNOWN_ACTION', message: 'Unknown action: spawn' }));
    expect(reply.success).toBe(false);
    expect(reply.versionMismatch).toBe('Version mismatch: test');
  });

  it('a successful one does not', async () => {
    const reply = await handleUnrealGatewayCall(spawn, mismatched({ success: true, actorName: 'StaticMeshActor_0' }));
    expect(reply.success).toBe(true);
    expect(reply.versionMismatch).toBeUndefined();
  });
});

describe('a call still running when the client would give up answers for itself', () => {
  const spawn = { operation: 'execute', tool: 'control_actor', action: 'spawn', params: { classPath: '/Script/Engine.StaticMeshActor' } };

  afterEach(() => {
    vi.useRealTimers();
  });

  it(`answers at ${STILL_RUNNING_AFTER_MS} ms with a running task, and the late result still replays under its idempotencyKey`, async () => {
    vi.useFakeTimers();
    let finish: (value: unknown) => void = () => undefined;
    const slow = new Promise((resolve) => { finish = resolve; });
    const context = gatewayContext(
      { isConnected: () => true, sendAutomationRequest: () => slow, getAuthority: () => ({ scopes: ['admin'] }) },
      'still-running'
    );
    const keyed = { ...spawn, options: { idempotencyKey: 'slow-spawn-1' } };

    const pending = handleUnrealGatewayCall(keyed, context);
    await vi.advanceTimersByTimeAsync(STILL_RUNNING_AFTER_MS);
    const early = await pending;
    expect(early.success).toBe(true);
    expect((early.receipt as { task?: { state?: string } }).task?.state).toBe('running');
    expect(String((early.data as { message?: string }).message)).toContain('same idempotencyKey');

    finish({ success: true, actorName: 'StaticMeshActor_0' });
    await vi.runAllTimersAsync();
    const replay = await handleUnrealGatewayCall(keyed, context);
    expect(replay.replayed).toBe(true);
    expect((replay.receipt as { task?: unknown }).task).toBeUndefined();
  });

  // A call without a key, or a client that lost it, had no way to the result but the log.
  it('a call without an idempotencyKey answers too, and get_task_result reads it running, then done', async () => {
    vi.useFakeTimers();
    let finish: (value: unknown) => void = () => undefined;
    const slow = new Promise((resolve) => { finish = resolve; });
    const context = gatewayContext(
      { isConnected: () => true, sendAutomationRequest: () => slow, getAuthority: () => ({ scopes: ['admin'] }) },
      'unkeyed-running'
    );
    const pending = handleUnrealGatewayCall(spawn, context);
    await vi.advanceTimersByTimeAsync(STILL_RUNNING_AFTER_MS);
    const early = await pending;
    const taskId = String((early.receipt as { task?: { taskId?: string } }).task?.taskId);
    expect(String((early.data as { message?: string }).message)).toContain('do not send this call again');
    const read = (): Promise<Record<string, unknown>> => handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_task_result', params: { taskId } }, context);
    expect(((await read()).data as { state?: string }).state).toBe('running');

    finish({ success: true, actorName: 'StaticMeshActor_0' });
    await vi.runAllTimersAsync();
    const done = (await read()).data as { state?: string; outcome?: { success?: boolean } };
    expect(done.state).toBe('done');
    expect(done.outcome?.success).toBe(true);
    const unknown = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_task_result', params: { taskId: 'no-such-task' } }, context);
    expect(unknown.errorCode).toBe('TASK_NOT_FOUND');
  });

  it.each([
    ['with an explicit timeoutMs', { options: { idempotencyKey: 'slow-spawn-2', timeoutMs: 120000 } }]
  ])(`a call %s waits past ${STILL_RUNNING_AFTER_MS} ms for its real result`, async (_label, extra) => {
    vi.useFakeTimers();
    let finish: (value: unknown) => void = () => undefined;
    const slow = new Promise((resolve) => { finish = resolve; });
    const context = gatewayContext(
      { isConnected: () => true, sendAutomationRequest: () => slow, getAuthority: () => ({ scopes: ['admin'] }) },
      `waits-${_label}`
    );
    let answered = false;
    const pending = handleUnrealGatewayCall({ ...spawn, ...extra }, context).then((reply) => { answered = true; return reply; });
    await vi.advanceTimersByTimeAsync(STILL_RUNNING_AFTER_MS + 1000);
    expect(answered).toBe(false);
    finish({ success: true, actorName: 'StaticMeshActor_0' });
    const reply = await pending;
    expect(reply.success).toBe(true);
    expect((reply.receipt as { task?: unknown }).task).toBeUndefined();
  });

  it('a call that finishes first answers with its result as before', async () => {
    const context = gatewayContext(
      { isConnected: () => true, sendAutomationRequest: async () => ({ success: true, actorName: 'StaticMeshActor_0' }), getAuthority: () => ({ scopes: ['admin'] }) },
      'fast'
    );
    const reply = await handleUnrealGatewayCall(spawn, context);
    expect(reply.success).toBe(true);
    expect((reply.receipt as { task?: unknown }).task).toBeUndefined();
  });
});
