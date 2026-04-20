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
    executeAutomationRequestMock.mockClear();
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
