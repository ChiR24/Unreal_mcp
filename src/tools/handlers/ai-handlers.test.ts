import { beforeEach, describe, expect, it, vi } from 'vitest';

import { handleAITools } from './ai-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

interface MockBridge {
  sendAutomationRequest: ReturnType<typeof vi.fn>;
  isConnected: () => boolean;
}

function makeMockTools(response: Record<string, unknown> = { success: true }): { tools: ITools; bridge: MockBridge } {
  const bridge: MockBridge = {
    sendAutomationRequest: vi.fn().mockResolvedValue(response),
    isConnected: () => true,
  };
  const tools = { automationBridge: bridge } as unknown as ITools;
  return { tools, bridge };
}

describe('manage_ai StateTree: create_state_tree', () => {
  it('forwards contextClass through to automation payload', async () => {
    const { tools, bridge } = makeMockTools({ success: true, stateTreePath: '/Game/ST' });
    await handleAITools(
      'create_state_tree',
      {
        name: 'ST_Ch7Test',
        path: '/Game/DataTest',
        contextClass: '/Script/GameplayStateTreeModule.StateTreeComponentSchema',
      } as unknown as Record<string, unknown>,
      tools
    );

    const calls = bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>;
    expect(calls.length).toBe(1);
    expect(calls[0][0]).toBe('manage_ai');
    const payload = calls[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('create_state_tree');
    expect(payload.contextClass).toBe('/Script/GameplayStateTreeModule.StateTreeComponentSchema');
    expect(payload.name).toBe('ST_Ch7Test');
    expect(payload.path).toBe('/Game/DataTest');
  });

  it('throws when name is missing', async () => {
    const { tools } = makeMockTools();
    await expect(
      handleAITools('create_state_tree', {} as unknown as Record<string, unknown>, tools)
    ).rejects.toThrow(/name/);
  });
});

describe('manage_ai StateTree: add_state_tree_state', () => {
  it('forwards stateType=Subtree + parentState', async () => {
    const { tools, bridge } = makeMockTools();
    await handleAITools(
      'add_state_tree_state',
      {
        stateTreePath: '/Game/DataTest/ST_Ch7Test',
        stateName: 'Combat',
        stateType: 'Subtree',
        parentState: 'Root',
      } as unknown as Record<string, unknown>,
      tools
    );

    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('add_state_tree_state');
    expect(payload.stateType).toBe('Subtree');
    expect(payload.parentState).toBe('Root');
  });

  it('forwards legacy parentStateName if provided', async () => {
    const { tools, bridge } = makeMockTools();
    await handleAITools(
      'add_state_tree_state',
      {
        stateTreePath: '/Game/ST',
        stateName: 'Leaf',
        parentStateName: 'Combat',
      } as unknown as Record<string, unknown>,
      tools
    );
    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.parentStateName).toBe('Combat');
  });
});

describe('manage_ai StateTree: configure_state_tree_task taskProps', () => {
  it('forwards taskProps + taskIndex to automation', async () => {
    const { tools, bridge } = makeMockTools();
    await handleAITools(
      'configure_state_tree_task',
      {
        stateTreePath: '/Game/DataTest/ST_Ch7Test',
        stateName: 'Combat',
        taskIndex: 0,
        taskProps: { Duration: 2.5, bLoop: true },
      } as unknown as Record<string, unknown>,
      tools
    );
    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('configure_state_tree_task');
    expect(payload.taskIndex).toBe(0);
    expect(payload.taskProps).toEqual({ Duration: 2.5, bLoop: true });
  });
});

describe('manage_ai StateTree: add_state_tree_task', () => {
  it('forwards stateTaskClass to automation', async () => {
    const { tools, bridge } = makeMockTools();
    await handleAITools(
      'add_state_tree_task',
      {
        stateTreePath: '/Game/ST',
        stateName: 'Combat',
        stateTaskClass: '/Script/StateTreeModule.StateTreeRunEnvQueryTask',
      } as unknown as Record<string, unknown>,
      tools
    );
    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('add_state_tree_task');
    expect(payload.stateTaskClass).toBe('/Script/StateTreeModule.StateTreeRunEnvQueryTask');
  });

  it('throws when stateTaskClass is missing', async () => {
    const { tools } = makeMockTools();
    await expect(
      handleAITools(
        'add_state_tree_task',
        { stateTreePath: '/Game/ST', stateName: 'X' } as unknown as Record<string, unknown>,
        tools
      )
    ).rejects.toThrow(/stateTaskClass/);
  });
});

describe('manage_ai StateTree: list_state_tree_states', () => {
  it('forwards stateTreePath and returns stateTreeTree', async () => {
    const { tools, bridge } = makeMockTools({
      success: true,
      stateTreeTree: { Root: { type: 'State', id: 'abc', children: {} } },
    });
    const res = await handleAITools(
      'list_state_tree_states',
      { stateTreePath: '/Game/DataTest/ST_Ch7Test' } as unknown as Record<string, unknown>,
      tools
    );
    expect(res.stateTreeTree).toBeDefined();
    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('list_state_tree_states');
  });
});

describe('manage_ai StateTree: remove_state_tree_state', () => {
  it('forwards stateTreePath + stateName', async () => {
    const { tools, bridge } = makeMockTools();
    await handleAITools(
      'remove_state_tree_state',
      {
        stateTreePath: '/Game/DataTest/ST_Ch7Test',
        stateName: 'Combat',
      } as unknown as Record<string, unknown>,
      tools
    );
    const payload = (bridge.sendAutomationRequest.mock.calls as unknown as Array<unknown[]>)[0][1] as Record<string, unknown>;
    expect(payload.subAction).toBe('remove_state_tree_state');
    expect(payload.stateName).toBe('Combat');
  });

  it('throws on missing stateName', async () => {
    const { tools } = makeMockTools();
    await expect(
      handleAITools(
        'remove_state_tree_state',
        { stateTreePath: '/Game/ST' } as unknown as Record<string, unknown>,
        tools
      )
    ).rejects.toThrow(/stateName/);
  });
});
