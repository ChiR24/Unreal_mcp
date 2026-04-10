import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../../../src/tools/handlers/common-handlers.js')>(
    '../../../src/tools/handlers/common-handlers.js'
  );

  return {
    ...actual,
    executeAutomationRequest: vi.fn()
  };
});

import { handleConsolidatedToolCall } from '../../../src/tools/consolidated-tool-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('Blueprint Public Route', () => {
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

  it('routes get_graph_details through the public manage_blueprint path with normalized blueprint and asset paths', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      graphName: 'EventGraph',
      nodeCount: 1,
      nodes: [{ nodeId: 'NODE_1', nodeName: 'EventBeginPlay', nodeTitle: 'Event BeginPlay' }]
    });

    await handleConsolidatedToolCall(
      'manage_blueprint',
      {
        action: 'get_graph_details',
        blueprintPath: '/Content/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        action: 'get_graph_details',
        subAction: 'get_graph_details',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph'
      }),
      'Automation bridge not available'
    );
  });

  it('routes get_pin_details through the public manage_blueprint path with nodeGuid compatibility', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      nodeId: '9EE1255348FD35239355998682FACD45',
      pins: [{ pinName: 'InString', direction: 'Input', pinType: 'String', linkedTo: [] }]
    });

    await handleConsolidatedToolCall(
      'manage_blueprint',
      {
        action: 'get_pin_details',
        assetPath: '/Content/IntegrationTest/BP_Inspection',
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
        action: 'get_pin_details',
        subAction: 'get_pin_details',
        assetPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeGuid: '9EE1255348FD35239355998682FACD45',
        nodeId: '9EE1255348FD35239355998682FACD45',
        pinName: 'InString'
      }),
      'Automation bridge not available'
    );
  });

  it('keeps mutating graph actions on the specialized graph route', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      nodeId: 'NODE_NEW'
    });

    await handleConsolidatedToolCall(
      'manage_blueprint',
      {
        action: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeType: 'Branch'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_Inspection',
        graphName: 'EventGraph',
        nodeType: 'K2Node_IfThenElse'
      }),
      'Automation bridge not available'
    );
  });
});