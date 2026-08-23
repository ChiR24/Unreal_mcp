import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
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

describe('handleGraphTools material root output resolution', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  async function connect(args: Record<string, unknown>): Promise<Record<string, unknown>> {
    await handleGraphTools('manage_material_graph', 'connect_nodes', args as never, {} as never);
    const call = executeAutomationRequestMock.mock.calls.at(-1) as unknown[] | undefined;
    return (call?.[2] ?? {}) as Record<string, unknown>;
  }

  // The root output is a UMaterialGraphNode_Root, not a UMaterialExpression, so the plugin
  // can only reach it through the 'Main' sentinel. Normalization used to apply solely to the
  // legacy toNodeId alias, so these spellings reached the bridge verbatim and failed as
  // NODE_NOT_FOUND — which left a material graph impossible to terminate.
  it.each([
    ['Root'],
    ['root'],
    ['MaterialGraphNode_Root_0'],
    ['MaterialGraphNode_Root'],
    ['MaterialOutput'],
    ['Material Output'],
    ['Output'],
    ['Material']
  ])('maps targetNodeId %j onto the Main sentinel', async (targetNodeId) => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      sourceNodeId: 'MaterialExpressionVectorParameter_0',
      targetNodeId,
      inputName: 'BaseColor'
    });

    expect(payload).toMatchObject({ targetNodeId: 'Main', inputName: 'BaseColor' });
  });

  it('treats a bare input pin with no target as the root', async () => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      sourceNodeId: 'MaterialExpressionMultiply_0',
      targetPin: 'Emissive Color'
    });

    expect(payload).toMatchObject({ targetNodeId: 'Main', inputName: 'EmissiveColor' });
  });

  // The plugin compares InputName with case-sensitive equality, so an off-case pin name
  // would otherwise fall through to "Unknown input on main node".
  it.each([
    ['basecolor', 'BaseColor'],
    ['worldPositionOffset', 'WorldPositionOffset'],
    ['ambient_occlusion', 'AmbientOcclusion'],
    ['AO', 'AmbientOcclusion']
  ])('canonicalizes root input pin %j to %j', async (inputName, expected) => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      sourceNodeId: 'MaterialExpressionScalarParameter_0',
      targetNodeId: 'Root',
      inputName
    });

    expect(payload).toMatchObject({ targetNodeId: 'Main', inputName: expected });
  });

  it('still honours the legacy toNodeId alias', async () => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      fromNodeId: 'MaterialExpressionVectorParameter_0',
      toNodeId: 'root',
      toPin: 'BaseColor'
    });

    expect(payload).toMatchObject({
      sourceNodeId: 'MaterialExpressionVectorParameter_0',
      targetNodeId: 'Main',
      inputName: 'BaseColor'
    });
  });

  it('leaves an ordinary expression target untouched', async () => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      sourceNodeId: 'MaterialExpressionVectorParameter_1',
      targetNodeId: 'MaterialExpressionMultiply_0',
      inputName: 'A'
    });

    expect(payload).toMatchObject({
      targetNodeId: 'MaterialExpressionMultiply_0',
      inputName: 'A'
    });
  });

  // 'Normal' is both a root input and a real input on several expressions, so the root pin
  // table must not rewrite pin names when the target is an expression.
  it('does not canonicalize pin names for expression targets', async () => {
    const payload = await connect({
      assetPath: '/Game/M_Test',
      sourceNodeId: 'MaterialExpressionVectorParameter_1',
      targetNodeId: 'MaterialExpressionCustom_0',
      inputName: 'normal'
    });

    expect(payload).toMatchObject({
      targetNodeId: 'MaterialExpressionCustom_0',
      inputName: 'normal'
    });
  });
});
