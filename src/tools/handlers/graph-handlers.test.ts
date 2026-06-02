import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleGraphTools } from './graph-handlers.js';

describe('handleGraphTools behavior tree payload mapping', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('normalizes behavior tree creation savePath aliases before dispatch', async () => {
    await handleGraphTools('manage_behavior_tree', 'create', {
      action: 'create',
      name: 'BT_Test',
      savePath: 'Game/MCPTest/BT'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_behavior_tree',
      expect.objectContaining({
        subAction: 'create',
        savePath: '/Game/MCPTest/BT'
      }),
      'Automation bridge not available'
    );
  });

  it('normalizes behavior tree assetPath aliases while preserving node aliases', async () => {
    await handleGraphTools('manage_behavior_tree', 'add_node', {
      action: 'add_node',
      assetPath: 'Game/MCPTest/BT/BT_Test',
      nodeType: 'Wait'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_behavior_tree',
      expect.objectContaining({
        subAction: 'add_node',
        assetPath: '/Game/MCPTest/BT/BT_Test',
        nodeType: 'BTTask_Wait',
        nodeCategory: 'task'
      }),
      'Automation bridge not available'
    );
  });

  it('forwards Enhanced Input action paths for blueprint node creation', async () => {
    await handleGraphTools('manage_blueprint', 'create_node', {
      action: 'create_node',
      blueprintPath: '/Game/BP_Player',
      nodeType: 'K2Node_EnhancedInputAction',
      actionPath: '/Game/Input/IA_Throttle'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_blueprint',
      expect.objectContaining({
        subAction: 'create_node',
        blueprintPath: '/Game/BP_Player',
        nodeType: 'K2Node_EnhancedInputAction',
        actionPath: '/Game/Input/IA_Throttle'
      }),
      'Automation bridge not available'
    );
  });
});
