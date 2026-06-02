import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleSequenceTools } from './sequence-handlers.js';

describe('handleSequenceTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('normalizes sequence creation path aliases before dispatch', async () => {
    await handleSequenceTools('create', {
      action: 'create',
      name: 'SEQ_Test',
      path: 'Game/MCPTest/Sequences'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'create',
        path: '/Game/MCPTest/Sequences'
      })
    );
  });

  it('normalizes duplicate source and destination path aliases before dispatch', async () => {
    await handleSequenceTools('duplicate', {
      action: 'duplicate',
      path: 'Game/MCPTest/Sequences/SEQ_Test',
      destinationPath: 'Content/MCPTest/Duplicates',
      newName: 'SEQ_Copy'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'duplicate',
        path: '/Game/MCPTest/Sequences/SEQ_Test',
        destinationPath: '/Game/MCPTest/Duplicates/SEQ_Copy'
      })
    );
  });
});
