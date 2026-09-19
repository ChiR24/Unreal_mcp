import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async (..._args: unknown[]): Promise<Record<string, unknown>> => ({ success: true, result: {} }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleMaterialAuthoringTools } from './material-authoring-handlers.js';

describe('handleMaterialAuthoringTools material pin mapping', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('keeps target pins as main material inputs', async () => {
    await handleMaterialAuthoringTools('connect_material_pins', {
      action: 'connect_material_pins',
      assetPath: '/Game/M_Test',
      nodeId: 'MaterialExpressionVectorParameter_0',
      sourcePin: '0',
      targetPin: 'BaseColor'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_material_authoring',
      expect.objectContaining({
        subAction: 'connect_nodes',
        assetPath: '/Game/M_Test',
        sourceNodeId: 'MaterialExpressionVectorParameter_0',
        sourcePin: '0',
        targetNodeId: 'Main',
        inputName: 'BaseColor'
      })
    );
  });

  it('splits a full material path into name and directory when creating materials', async () => {
    await handleMaterialAuthoringTools('create_material', {
      action: 'create_material',
      materialPath: '/Game/MCPTest/M_Test',
      domain: 'PostProcess',
      blendMode: 'Translucent',
      shadingModel: 'Unlit',
      twoSided: true,
      save: false
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_material_authoring',
      {
        subAction: 'create_material',
        name: 'M_Test',
        path: '/Game/MCPTest',
        materialDomain: 'PostProcess',
        blendMode: 'Translucent',
        shadingModel: 'Unlit',
        twoSided: true,
        save: false
      }
    );
  });

  it('maps instance path and parent aliases when creating material instances', async () => {
    await handleMaterialAuthoringTools('create_material_instance', {
      action: 'create_material_instance',
      instancePath: '/Game/MCPTest/MI_Test',
      parentMaterialPath: '/Game/MCPTest/M_Test',
      save: false
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_material_authoring',
      {
        subAction: 'create_material_instance',
        name: 'MI_Test',
        path: '/Game/MCPTest',
        parentMaterial: '/Game/MCPTest/M_Test',
        save: false
      }
    );
  });

  it('rejects non-array custom expression inputs before automation', async () => {
    const response = await handleMaterialAuthoringTools('add_custom_expression', {
      action: 'add_custom_expression',
      assetPath: '/Game/MCPTest/M_Test',
      code: 'return 0;',
      inputs: { name: 'Bad' }
    }, {} as never);

    expect(response).toMatchObject({
      success: false,
      isError: true,
      message: 'manage_material_authoring.add_custom_expression: inputs must be an array'
    });
    expect(executeAutomationRequestMock).not.toHaveBeenCalled();
  });

  it('promotes returned node ids when adding material nodes', async () => {
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: true,
      message: 'created',
      result: { nodeId: 'MaterialExpressionMultiply_1' }
    });

    const response = await handleMaterialAuthoringTools('add_material_node', {
      action: 'add_material_node',
      assetPath: '/Game/MCPTest/M_Test',
      nodeType: 'Multiply',
      posX: 120,
      posY: 240
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_material_authoring',
      {
        subAction: 'add_material_node',
        assetPath: '/Game/MCPTest/M_Test',
        nodeType: 'Multiply',
        x: 120,
        y: 240
      }
    );
    expect(response).toMatchObject({
      success: true,
      message: 'created',
      nodeId: 'MaterialExpressionMultiply_1'
    });
  });

  it('routes rebuild material through compile material payloads', async () => {
    await handleMaterialAuthoringTools('rebuild_material', {
      action: 'rebuild_material',
      materialPath: '/Game/MCPTest/M_Test',
      save: false
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_material_authoring',
      {
        subAction: 'compile_material',
        assetPath: '/Game/MCPTest/M_Test',
        save: false
      }
    );
  });
});
