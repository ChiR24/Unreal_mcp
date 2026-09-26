import { describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return { ...actual, executeAutomationRequest: executeAutomationRequestMock };
});

import { handleBlueprintTools } from './blueprint-handlers.js';

describe('add_node payload', () => {
  // add_node declares memberName; the stdio door forwarded only functionName, so
  // {nodeType:'VariableGet', memberName:'Label'} reached the plugin nameless.
  it('forwards memberName to the plugin', async () => {
    await handleBlueprintTools('add_node', {
      action: 'add_node', blueprintPath: '/Game/BP_Test', nodeType: 'VariableGet', memberName: 'Label'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'manage_blueprint',
      expect.objectContaining({ subAction: 'create_node', nodeType: 'K2Node_VariableGet', memberName: 'Label' }),
      undefined
    );
  });
});
