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
});
