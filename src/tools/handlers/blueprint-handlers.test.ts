import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async (): Promise<Record<string, unknown>> => ({ success: true, nodes: [] }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleBlueprintTools } from './blueprint-handlers.js';

describe('manage_blueprint list_graph_nodes', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
  });


  it('forwards blueprintPath + graphName to bridge', async () => {
    await handleBlueprintTools(
      'list_graph_nodes',
      {
        action: 'list_graph_nodes',
        blueprintPath: '/Game/UI/WBP_Foo',
        graphName: 'EventGraph'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'list_nodes',
        assetPath: '/Game/UI/WBP_Foo',
        graphName: 'EventGraph'
      })
    );
  });

  it('forwards optional nameFilter and classFilter', async () => {
    await handleBlueprintTools(
      'list_graph_nodes',
      {
        action: 'list_graph_nodes',
        blueprintPath: '/Game/Blueprints/BP_Foo',
        graphName: 'EventGraph',
        nameFilter: 'OnPaint',
        classFilter: 'K2Node_CallFunction'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'list_nodes',
        nameFilter: 'OnPaint',
        classFilter: 'K2Node_CallFunction'
      })
    );
  });
});

describe('manage_blueprint list_interfaces', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: true,
      interfaces: ['/Game/IF_Test.IF_Test_C']
    });
  });

  it('forwards list_interfaces action to automation with blueprintPath', async () => {
    const res = await handleBlueprintTools(
      'list_interfaces',
      { action: 'list_interfaces', blueprintPath: '/Game/BP_Test' },
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.interfaces).toEqual(['/Game/IF_Test.IF_Test_C']);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'blueprint_list_interfaces',
      expect.objectContaining({ blueprintPath: '/Game/BP_Test' })
    );
  });

  it('throws on missing blueprintPath', async () => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
    await expect(handleBlueprintTools(
      'list_interfaces',
      { action: 'list_interfaces' },
      {} as never
    )).rejects.toThrow(/blueprintPath/);
  });
});

describe('manage_blueprint add_interface', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
  });

  it('forwards add_interface with blueprintPath and interfacePath', async () => {
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: true,
      currentInterfaces: ['/Game/IF_Test.IF_Test_C']
    });
    const res = await handleBlueprintTools(
      'add_interface',
      { action: 'add_interface', blueprintPath: '/Game/BP_Test', interfacePath: '/Game/IF_Test.IF_Test_C' },
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.currentInterfaces).toEqual(['/Game/IF_Test.IF_Test_C']);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'blueprint_add_interface',
      expect.objectContaining({
        blueprintPath: '/Game/BP_Test',
        interfacePath: '/Game/IF_Test.IF_Test_C'
      })
    );
  });

  it('throws on missing interfacePath', async () => {
    await expect(handleBlueprintTools(
      'add_interface',
      { action: 'add_interface', blueprintPath: '/Game/BP_Test' },
      {} as never
    )).rejects.toThrow(/interfacePath/);
  });
});

describe('manage_blueprint remove_interface', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
  });

  it('forwards remove_interface with both paths', async () => {
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: true,
      currentInterfaces: []
    });
    const res = await handleBlueprintTools(
      'remove_interface',
      { action: 'remove_interface', blueprintPath: '/Game/BP_T', interfacePath: '/Script/Engine.Interface_AssetUserData' },
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.currentInterfaces).toEqual([]);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'blueprint_remove_interface',
      expect.objectContaining({
        blueprintPath: '/Game/BP_T',
        interfacePath: '/Script/Engine.Interface_AssetUserData'
      })
    );
  });
});

describe('manage_blueprint set_parent_class', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, nodes: [] });
  });

  it('forwards set_parent_class with blueprintPath and parentClass', async () => {
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: true,
      oldParent: '/Script/Engine.Actor',
      newParent: '/Script/Engine.Pawn'
    });
    const res = await handleBlueprintTools(
      'set_parent_class',
      { action: 'set_parent_class', blueprintPath: '/Game/BP_T', parentClass: '/Script/Engine.Pawn' },
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.oldParent).toBe('/Script/Engine.Actor');
    expect(res.newParent).toBe('/Script/Engine.Pawn');
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'blueprint_set_parent_class',
      expect.objectContaining({
        blueprintPath: '/Game/BP_T',
        parentClass: '/Script/Engine.Pawn'
      })
    );
  });

  it('throws on missing parentClass', async () => {
    await expect(handleBlueprintTools(
      'set_parent_class',
      { action: 'set_parent_class', blueprintPath: '/Game/BP_T' },
      {} as never
    )).rejects.toThrow(/parentClass/);
  });
});
