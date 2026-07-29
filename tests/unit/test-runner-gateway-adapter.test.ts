import { describe, expect, it, vi } from 'vitest';

import { createToolCaller, toGatewayCall } from '../test-runner.mjs';

describe('test runner gateway adapter', () => {
  it('maps a direct tool call without mutating its arguments', () => {
    const args = {
      action: 'spawn_actor',
      params: {
        actorClass: '/Script/Engine.StaticMeshActor',
        name: 'NestedName',
      },
      name: 'FlatName',
      timeoutMs: 12_000,
    };
    const original = structuredClone(args);

    expect(toGatewayCall('control_actor', args)).toEqual({
      name: 'unreal',
      arguments: {
        operation: 'execute',
        tool: 'control_actor',
        action: 'spawn_actor',
        params: {
          actorClass: '/Script/Engine.StaticMeshActor',
          name: 'FlatName',
        },
        options: { timeoutMs: 12_000 },
      },
    });
    expect(args).toEqual(original);
  });

  it('adapts direct calls at the client boundary and nests the harness timeout', async () => {
    const callTool = vi.fn(async () => ({ structuredContent: { success: true } }));
    const callToolOnce = createToolCaller({ callTool });

    await callToolOnce(
      {
        name: 'control_actor',
        arguments: {
          action: 'spawn_actor',
          actorClass: '/Script/Engine.StaticMeshActor',
        },
      },
      5_000,
    );

    expect(callTool).toHaveBeenCalledOnce();
    expect(callTool).toHaveBeenCalledWith(
      {
        name: 'unreal',
        arguments: {
          operation: 'execute',
          tool: 'control_actor',
          action: 'spawn_actor',
          params: { actorClass: '/Script/Engine.StaticMeshActor' },
          options: { timeoutMs: 5_000 },
        },
      },
      undefined,
      { timeout: 5_000 },
    );
  });

  it('preserves an already-gateway execute call and its explicit timeout', async () => {
    const callTool = vi.fn(async () => ({ structuredContent: { success: true } }));
    const callToolOnce = createToolCaller({ callTool });
    const gatewayCall = {
      name: 'unreal',
      arguments: {
        operation: 'execute',
        tool: 'control_actor',
        action: 'spawn_actor',
        params: { actorClass: '/Script/Engine.StaticMeshActor' },
        options: { timeoutMs: 7_000 },
      },
    };

    await callToolOnce(gatewayCall);

    expect(callTool).toHaveBeenCalledWith(gatewayCall, undefined, { timeout: 7_000 });
  });

  it('rejects a direct call that has no action selector', () => {
    expect(() => toGatewayCall('control_actor', { actorClass: '/Script/Engine.Actor' }))
      .toThrow(TypeError);
  });
});
