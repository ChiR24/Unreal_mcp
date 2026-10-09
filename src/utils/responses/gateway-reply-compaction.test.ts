import { describe, expect, it } from 'vitest';
import { compactGatewayReply } from './gateway-reply-compaction.js';

// Shapes copied from live native replies (UE 5.8), trimmed to the fields the rules touch.
const live = { selection: 3, level: 3, assetRegistry: 10890, package: 1, serverInstanceId: 'c4f3b5a5' };
const bookkeeping = {
  catalogRevision: 'bfcee38a7ffedc73',
  capabilityRevision: '9e79281f928735f1c48a84bbc0fa3b8b768dfc65ec74981675f4100fbfa1eb60',
  schemaRevision: 'cec5136436a32511b50a1155aec7af913feeb7da8407d8a505e105cbfc5ae972',
  correlationId: '69FC1EA9-4FA0-3D47-4B23-0A9B56AD0264'
};

const setProperty = {
  capabilityId: 'inspect.set_property', tool: 'inspect', action: 'set_property', ...bookkeeping,
  status: 'success', liveRevisions: live, success: true, operation: 'execute', message: 'Actor visibility updated.',
  receipt: {
    status: 'success', capabilityId: 'inspect.set_property', requestId: 'num:63', ...bookkeeping, timingMs: 287,
    nextCalls: [], handles: [{ kind: 'actor', ref: 'Courier' }], changes: ['Courier'], warnings: [],
    validation: { outputSchema: 'passed' }, dataDigest: 'sha1:29a4', liveRevisions: live
  },
  data: { success: true, message: 'Actor visibility updated.', details: { propertyName: 'bHidden', value: false } }
};

describe('compactGatewayReply', () => {
  it('keeps what an execute did and drops what only logs read', () => {
    expect(compactGatewayReply(setProperty)).toEqual({
      success: true, operation: 'execute', message: 'Actor visibility updated.',
      receipt: { handles: [{ kind: 'actor', ref: 'Courier' }], changes: ['Courier'] },
      data: { details: { propertyName: 'bHidden', value: false } }
    });
    expect(setProperty.receipt.correlationId, 'the input is left whole').toBe(bookkeeping.correlationId);
  });

  it('lists a warning once, in the receipt, and keeps the engine counts', () => {
    const line = '[LogBlueprint] Get SkeletalMesh is deprecated';
    const loaded = compactGatewayReply({
      success: true, operation: 'execute', message: 'Level loaded',
      receipt: { warnings: [line], nextCalls: [] },
      data: { success: true, message: 'Level loaded', details: { engineWarningCount: 5, warnings: [line] } }
    });
    expect(loaded).toEqual({
      success: true, operation: 'execute', message: 'Level loaded',
      receipt: { warnings: [line] }, data: { details: { engineWarningCount: 5 } }
    });
  });

  it('drops a payload list or path the reply already states, and the raw result beside its projection', () => {
    const shown = compactGatewayReply({
      success: true, operation: 'execute', message: 'Compiled.',
      receipt: { changes: ['/Game/UI/WBP_Menu'] },
      data: { widgetPath: '/Game/UI/WBP_Menu', details: { changedAssets: ['/Game/UI/WBP_Menu'], assetPath: '/Game/UI/WBP_Menu', assetName: 'WBP_Menu' } },
      result: { success: true, widgetPath: '/Game/UI/WBP_Menu', assetName: 'WBP_Menu' }
    });
    expect(shown).toEqual({
      success: true, operation: 'execute', message: 'Compiled.', receipt: { changes: ['/Game/UI/WBP_Menu'] },
      data: { widgetPath: '/Game/UI/WBP_Menu', details: { assetName: 'WBP_Menu' } }
    });
    const spawned = compactGatewayReply({
      success: true, operation: 'execute',
      data: { name: 'DynamicMeshActor_3', class: 'DynamicMeshActor', details: { actorName: 'DynamicMeshActor_3', actorClass: 'DynamicMeshActor', saved: 'no', phase: 'none' } }
    }) as { data: unknown };
    expect(spawned.data, 'a short value is kept even when another field shares it').toEqual({
      name: 'DynamicMeshActor_3', class: 'DynamicMeshActor', details: { saved: 'no', phase: 'none' }
    });
    const unlisted = compactGatewayReply({
      success: true, operation: 'execute', data: { details: { changedAssets: ['/Game/A'] } }
    }) as { data: unknown };
    expect(unlisted.data, 'a change the receipt does not list stays').toEqual({ details: { changedAssets: ['/Game/A'] } });
    const many = compactGatewayReply({
      success: true, operation: 'execute', receipt: { changes: ['/Game/Maps/Port'] },
      data: { applied: 2, details: { changedEntities: ['/Game/Maps/Port'] } }
    }) as { data: unknown };
    expect(many.data, 'the entities a many-object write names, once the receipt lists them').toEqual({ applied: 2 });
    const removed = compactGatewayReply({
      success: true, operation: 'execute', receipt: { changes: ['/Game/A', '/Game/B'] },
      data: { details: { deletedCount: 2, deleted: ['/Game/A', '/Game/B'] } }
    }) as { data: unknown };
    expect(removed.data, 'a delete names its paths once, in the receipt').toEqual({ details: { deletedCount: 2 } });
    const failed = compactGatewayReply({ success: false, operation: 'execute', message: 'No.', result: { missing: ['/Game/B'] } });
    expect(failed, 'a failure keeps its detail').toMatchObject({ result: { missing: ['/Game/B'] } });
  });

  it('reduces a stdio refusal\'s bridge frame to what it alone says', () => {
    const refusal = compactGatewayReply({
      success: false, operation: 'execute', errorCode: 'ACTOR_NOT_FOUND', message: 'Actor not found',
      result: {
        type: 'automation_response', requestId: 'r-1', success: false, message: 'Actor not found', error: 'ACTOR_NOT_FOUND',
        result: { success: false, data: {}, error: { code: 'ACTOR_NOT_FOUND', message: 'Actor not found' }, missing: ['X'] },
        liveRevisions: { level: 3 }
      }
    });
    expect(refusal).toEqual({
      success: false, operation: 'execute', errorCode: 'ACTOR_NOT_FOUND', message: 'Actor not found', result: { result: { missing: ['X'] } }
    });
    const refused = compactGatewayReply({
      success: false, operation: 'execute', errorCode: 'OUTPUT_SCHEMA_VIOLATION', message: 'settings must be an object.',
      result: { success: true, message: 'Handler completed.', settings: 'bad' }
    }) as { result: unknown };
    expect(refused.result, 'the handler\'s own success claim stays beside the refusal').toEqual({ success: true, message: 'Handler completed.', settings: 'bad' });
  });

  it('drops the stdio contract\'s catalog bookkeeping and keeps its execute nextCall', () => {
    const contract = compactGatewayReply({
      success: true, operation: 'describe', scope: 'capability', capability: 'inspect.get_property', parentTool: 'inspect',
      action: 'get_property', category: 'core', topics: ['read property'], aliases: [], perActionSchemas: true,
      legacyIds: [{ tool: 'inspect', action: 'get_property' }], migratedFrom: { tool: 'inspect', action: 'get_property' },
      parameters: [{ name: 'actorName', type: 'string', description: 'Actor.' }], parameterCount: 1, runnable: true,
      nextCall: { operation: 'execute', capability: 'inspect.get_property', tool: 'inspect', action: 'get_property', params: {} }
    });
    expect(contract).toEqual({
      success: true, operation: 'describe', scope: 'capability', capability: 'inspect.get_property', parentTool: 'inspect',
      action: 'get_property', parameters: [{ name: 'actorName', type: 'string', description: 'Actor.' }],
      nextCall: { operation: 'execute', capability: 'inspect.get_property', tool: 'inspect', action: 'get_property', params: {} }
    });
  });

  it('says an engine refusal once and keeps the guidance', () => {
    const refusal = compactGatewayReply({
      capabilityId: 'control_actor.set_material', ...bookkeeping, status: 'error', liveRevisions: live,
      success: false, operation: 'execute', message: 'Actor not found', error: 'Actor not found', errorCode: 'ACTOR_NOT_FOUND',
      typedError: {
        kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'Actor not found', handlerCode: 'ACTOR_NOT_FOUND',
        unrealDetail: { success: false, data: {}, error: { code: 'ACTOR_NOT_FOUND', message: 'Actor not found' } }
      },
      receipt: { status: 'error', nextCalls: [], error: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'Actor not found' } },
      suggestions: ["No actor is labeled 'X'"],
      nextCall: { operation: 'execute', tool: 'control_actor', action: 'find', params: { findBy: 'name', name: 'X' } }
    });
    expect(refusal).toEqual({
      success: false, operation: 'execute', message: 'Actor not found', errorCode: 'ACTOR_NOT_FOUND',
      typedError: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR' },
      suggestions: ["No actor is labeled 'X'"],
      nextCall: { operation: 'execute', tool: 'control_actor', action: 'find', params: { findBy: 'name', name: 'X' } }
    });
  });

  it('keeps partial results, a running task and a typed error no other field carries', () => {
    const partial = compactGatewayReply({
      success: false, operation: 'execute', message: 'stopped at operations[10]', errorCode: 'EXPRESSION_INVALID',
      typedError: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', retryable: false, unrealDetail: { results: [{ index: 0 }] } },
      receipt: { error: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'stopped at operations[10]', retryable: false } }
    }) as Record<string, unknown>;
    expect(partial.typedError).toEqual({ kind: 'execution', code: 'UNREAL_ENGINE_ERROR', retryable: false, unrealDetail: { results: [{ index: 0 }] } });
    expect(partial.receipt).toBeUndefined();
    // Over stdio there is no typedError: the receipt's error is the only typed one, so it stays.
    const stdio = compactGatewayReply({
      success: false, operation: 'execute', message: 'busy', errorCode: 'NOT_CONNECTED',
      receipt: { error: { kind: 'dispatch', code: 'NOT_CONNECTED', message: 'busy', retryable: true } }
    }) as Record<string, unknown>;
    expect(stdio.receipt).toEqual({ error: { kind: 'dispatch', code: 'NOT_CONNECTED', retryable: true } });
    const running = compactGatewayReply({ success: true, operation: 'execute', receipt: { task: { taskId: 'r1', state: 'running' } } });
    expect(running).toEqual({ success: true, operation: 'execute', receipt: { task: { taskId: 'r1', state: 'running' } } });
  });

  it('keeps a contract whole while saying each description once', () => {
    const contract = compactGatewayReply({
      action: 'delete', capability: 'sequence.delete', catalogRevision: 'bfcee38a7ffedc73', effect: 'destructive',
      behavior: { effect: 'destructive', idempotency: 'non-idempotent', longRunning: false, safeToRetry: false },
      exampleCount: 1, examples: [{ title: 'Delete a sequence.', input: { path: '/Game/S' } }],
      hashes: { algorithm: 'sha256', schema: 'a', content: 'b' },
      inputSchema: {
        $schema: 'https://json-schema.org/draft/2020-12/schema', type: 'object', required: ['path'],
        properties: {
          path: { type: 'string', description: 'Sequence path.' },
          watch: { type: 'object', description: 'Sample a value.', properties: { durationSeconds: { type: 'number', description: 'Seconds.' } } }
        }
      },
      message: 'Exact capability contract.', operation: 'describe',
      outputSchema: {
        $schema: 'https://json-schema.org/draft/2020-12/schema', type: 'object', required: ['success'],
        properties: { success: { type: 'boolean', description: 'Whether the action succeeded.' }, removedKeys: { type: 'integer', description: 'Keys removed.' } }
      },
      parameters: [{ name: 'path', required: true, type: 'string', description: 'Sequence path.' }, { name: 'watch', type: 'object', description: 'Sample a value.' }],
      parent: 'manage_sequence', scope: 'capability', success: true, summary: 'Delete a sequence.', tool: 'manage_sequence'
    });
    expect(contract).toEqual({
      action: 'delete', capability: 'sequence.delete', catalogRevision: 'bfcee38a7ffedc73', effect: 'destructive',
      behavior: { idempotency: 'non-idempotent', longRunning: false, safeToRetry: false },
      examples: [{ input: { path: '/Game/S' } }],
      inputSchema: {
        type: 'object', required: ['path'],
        properties: {
          path: { type: 'string' },
          watch: { type: 'object', properties: { durationSeconds: { type: 'number', description: 'Seconds.' } } }
        }
      },
      operation: 'describe',
      outputSchema: { type: 'object', properties: { removedKeys: { type: 'integer', description: 'Keys removed.' } } },
      parameters: [{ name: 'path', required: true, type: 'string', description: 'Sequence path.' }, { name: 'watch', type: 'object', description: 'Sample a value.' }],
      scope: 'capability', success: true, summary: 'Delete a sequence.', tool: 'manage_sequence'
    });
    const stockOnly = compactGatewayReply({
      operation: 'describe', scope: 'capability', success: true,
      outputSchema: { type: 'object', properties: { success: { type: 'boolean', description: 'Whether the action succeeded.' } } }
    });
    expect(stockOnly).toEqual({ operation: 'describe', scope: 'capability', success: true });
    const restart = compactGatewayReply({
      operation: 'describe', scope: 'capability', success: true,
      outputSchema: {
        type: 'object', required: ['success', 'restarting'],
        properties: { success: { type: 'boolean', description: 'Whether the action succeeded.' }, restarting: { type: 'boolean' } }
      }
    }) as Record<string, unknown>;
    expect(restart.outputSchema, 'required names only what is still listed').toEqual({
      type: 'object', required: ['restarting'], properties: { restarting: { type: 'boolean' } }
    });
  });

  it('keeps search rows to what picks one', () => {
    const found = compactGatewayReply({
      catalogRevision: 'bfcee38a7ffedc73', hasMore: true, limit: 3, offset: 0, operation: 'search', query: 'set property',
      message: 'Results are capability-level and bounded.', success: true, total: 127, nextCursor: '3',
      results: [{
        available: true, capability: 'inspect.set_property', domain: 'inspect', effect: 'write', family: 'property',
        matchReasons: ['id-exact'], score: 474, parent: 'inspect', summary: 'Write a property value.',
        nextCall: { operation: 'describe', tool: 'inspect', action: 'set_property' }
      }]
    });
    expect(found).toEqual({
      catalogRevision: 'bfcee38a7ffedc73', hasMore: true, limit: 3, offset: 0, operation: 'search', success: true, total: 127, nextCursor: '3',
      results: [{
        available: true, capability: 'inspect.set_property', domain: 'inspect', effect: 'write', family: 'property',
        summary: 'Write a property value.', nextCall: { operation: 'describe', tool: 'inspect', action: 'set_property' }
      }]
    });
  });

  it('leaves configure replies and failed discovery messages alone', () => {
    const configure = { success: true, operation: 'configure', action: 'list_tools', result: { tools: [] } };
    expect(compactGatewayReply(configure)).toBe(configure);
    const failed = { success: false, operation: 'search', message: 'query is required', errorCode: 'INVALID_QUERY' };
    expect(compactGatewayReply(failed)).toEqual(failed);
  });
});
