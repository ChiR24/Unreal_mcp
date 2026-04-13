import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn()
}));

import { handleBlueprintTools, handleBlueprintGet } from '../../../src/tools/handlers/blueprint-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('Blueprint Handlers', () => {
  const mockExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn()
      }
    } as unknown as ITools;
  });

  it('preserves rich blueprint_get variable details in wrapped response', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      message: 'Blueprint fetched',
      resolvedPath: '/Game/Abilities/Shared/AS_CharacterStats',
      defaults: {
        Strength_Min: '0.0',
        StatsComponent: 'None'
      },
      variables: [
        {
          name: 'Strength_Min',
          type: 'float',
          inherited: false,
          metadata: { tooltip: 'Minimum strength' }
        },
        {
          name: 'StatsComponent',
          type: 'UActorComponent*',
          component: true,
          inherited: true,
          declaredInBlueprintPath: '/Game/Abilities/Base/BP_BaseStats'
        }
      ]
    });

    const result = await handleBlueprintGet(
      { blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'blueprint_get',
      { blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats' },
      'Automation bridge not available for blueprint operations'
    );
    expect(result).toEqual({
      success: true,
      message: 'Blueprint fetched',
      blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats',
      blueprint: {
        resolvedPath: '/Game/Abilities/Shared/AS_CharacterStats',
        defaults: {
          Strength_Min: '0.0',
          StatsComponent: 'None'
        },
        variables: [
          {
            name: 'Strength_Min',
            type: 'float',
            inherited: false,
            metadata: { tooltip: 'Minimum strength' }
          },
          {
            name: 'StatsComponent',
            type: 'UActorComponent*',
            component: true,
            inherited: true,
            declaredInBlueprintPath: '/Game/Abilities/Base/BP_BaseStats'
          }
        ]
      }
    });
  });

  it('forwards get_graph_details with normalized blueprint and asset paths', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      type: 'automation_response',
      requestId: 'REQ_GRAPH_DETAILS',
      success: true,
      message: 'Graph details retrieved.',
      error: '',
      result: {
        graphName: 'EventGraph',
        nodeCount: 2,
        nodes: [
          {
            nodeId: 'NODE_1',
            nodeName: 'EventBeginPlay',
            nodeTitle: 'Event BeginPlay'
          }
        ],
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        assetName: 'BP_Inspection',
        existsAfter: true,
        assetClass: 'Blueprint'
      }
    });

    const result = await handleBlueprintTools(
      'get_graph_details',
      {
        blueprintPath: '/Content/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'get_graph_details',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph'
      }),
      'Automation bridge not available'
    );

    expect(result).toEqual({
      type: 'automation_response',
      requestId: 'REQ_GRAPH_DETAILS',
      success: true,
      message: 'Graph details retrieved.',
      error: '',
      result: {
        graphName: 'EventGraph',
        nodeCount: 2,
        nodes: [
          {
            nodeId: 'NODE_1',
            nodeName: 'EventBeginPlay',
            nodeTitle: 'Event BeginPlay'
          }
        ],
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        assetName: 'BP_Inspection',
        existsAfter: true,
        assetClass: 'Blueprint'
      },
      graphName: 'EventGraph',
      nodeCount: 2,
      nodes: [
        {
          nodeId: 'NODE_1',
          nodeName: 'EventBeginPlay',
          nodeTitle: 'Event BeginPlay'
        }
      ],
      assetPath: '/Game/IntegrationTest/BP_Inspection',
      assetName: 'BP_Inspection',
      existsAfter: true,
      assetClass: 'Blueprint'
    });
  });

  it('aliases nodeGuid to nodeId and preserves structured pin details', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      type: 'automation_response',
      requestId: 'REQ_PIN_DETAILS',
      success: true,
      message: 'Pin details retrieved.',
      error: '',
      result: {
        nodeId: '9EE1255348FD35239355998682FACD45',
        pins: [
          {
            pinName: 'InString',
            direction: 'Input',
            pinType: 'String',
            linkedTo: []
          }
        ]
      }
    });

    const result = await handleBlueprintTools(
      'get_pin_details',
      {
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeGuid: '9EE1255348FD35239355998682FACD45',
        pinName: 'InString'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'get_pin_details',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeGuid: '9EE1255348FD35239355998682FACD45',
        nodeId: '9EE1255348FD35239355998682FACD45',
        pinName: 'InString'
      }),
      'Automation bridge not available'
    );

    expect(result).toEqual({
      type: 'automation_response',
      requestId: 'REQ_PIN_DETAILS',
      success: true,
      message: 'Pin details retrieved.',
      error: '',
      result: {
        nodeId: '9EE1255348FD35239355998682FACD45',
        pins: [
          {
            pinName: 'InString',
            direction: 'Input',
            pinType: 'String',
            linkedTo: []
          }
        ]
      },
      nodeId: '9EE1255348FD35239355998682FACD45',
      pins: [
        {
          pinName: 'InString',
          direction: 'Input',
          pinType: 'String',
          linkedTo: []
        }
      ]
    });
  });

  it('forwards get_node_details_batch without reordering caller supplied nodeIds', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      shown: 2,
      totalRequested: 3,
      truncated: true,
      nextCursor: 'cursor:2',
      nodes: [
        {
          nodeId: 'NODE_2',
          nodeTitle: 'Print String',
          pins: []
        },
        {
          nodeId: 'NODE_1',
          nodeTitle: 'Event BeginPlay',
          pins: []
        }
      ]
    });

    const result = await handleBlueprintTools(
      'get_node_details_batch',
      {
        blueprintPath: '/Content/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeIds: ['NODE_2', 'NODE_1', 'NODE_3'],
        limit: 2,
        cursor: 'cursor:0'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'get_node_details_batch',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeIds: ['NODE_2', 'NODE_1', 'NODE_3'],
        limit: 2,
        cursor: 'cursor:0'
      }),
      'Automation bridge not available'
    );

    expect(result).toEqual({
      success: true,
      shown: 2,
      totalRequested: 3,
      truncated: true,
      nextCursor: 'cursor:2',
      nodes: [
        {
          nodeId: 'NODE_2',
          nodeTitle: 'Print String',
          pins: []
        },
        {
          nodeId: 'NODE_1',
          nodeTitle: 'Event BeginPlay',
          pins: []
        }
      ]
    });
  });

  it('forwards get_graph_review_summary with normalized blueprint and asset paths', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      graphName: 'ReviewFunction',
      nodeCount: 4,
      connectionCount: 6,
      entryNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry' }],
      commentGroups: [],
      highFanOutNodes: [],
      reviewTargets: [
        { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' }
      ],
      truncated: false,
      focusedReviewTarget: { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' },
      incomingNodes: [],
      outgoingNodes: [],
      containingCommentGroup: null,
      focusTruncated: false
    });

    const result = await handleBlueprintTools(
      'get_graph_review_summary',
      {
        blueprintPath: '/Content/IntegrationTest/BP_Inspection',
        graphName: 'ReviewFunction',
        nodeId: 'NODE_ENTRY'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'get_graph_review_summary',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'ReviewFunction',
        nodeId: 'NODE_ENTRY'
      }),
      'Automation bridge not available'
    );

    expect(result).toEqual({
      success: true,
      graphName: 'ReviewFunction',
      nodeCount: 4,
      connectionCount: 6,
      entryNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry' }],
      commentGroups: [],
      highFanOutNodes: [],
      reviewTargets: [
        { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' }
      ],
      truncated: false,
      focusedReviewTarget: { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' },
      incomingNodes: [],
      outgoingNodes: [],
      containingCommentGroup: null,
      focusTruncated: false
    });
  });

  it('delegates graph inspection actions through handleGraphTools with canonical manage_blueprint routing', async () => {
    vi.resetModules();

    const mockHandleGraphTools = vi.fn().mockResolvedValue({
      success: true,
      graphName: 'EventGraph',
      nodeCount: 1,
    });

    vi.doMock('../../../src/tools/handlers/graph-handlers.js', () => ({
      handleGraphTools: mockHandleGraphTools,
    }));

    vi.doMock('../../../src/tools/handlers/common-handlers.js', () => ({
      executeAutomationRequest: vi.fn(),
    }));

    const { handleBlueprintTools: handleBlueprintToolsWithRouteMock } = await import('../../../src/tools/handlers/blueprint-handlers.js');

    const routeMockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn(),
      },
    } as unknown as ITools;

    await handleBlueprintToolsWithRouteMock(
      'get_graph_details',
      {
        blueprintPath: '/Content/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
      },
      routeMockTools,
    );

    expect(mockHandleGraphTools).toHaveBeenCalledWith(
      'manage_blueprint',
      'get_graph_details',
      expect.objectContaining({
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
      }),
      routeMockTools,
    );
  });
});
