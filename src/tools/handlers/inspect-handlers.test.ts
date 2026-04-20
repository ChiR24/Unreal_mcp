import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, functions: [], properties: [] }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleInspectTools } from './inspect-handlers.js';

describe('inspect_class detailed reflection', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('forwards detailed/includeInherited/functionFilter to bridge', async () => {
    await handleInspectTools(
      'inspect_class',
      {
        action: 'inspect_class',
        className: '/Game/Blueprints/Foo.Foo_C',
        detailed: true,
        includeInherited: true,
        functionFilter: 'OnPaint',
        functionFlagFilter: ['FUNC_BlueprintEvent'],
        propertyFilter: 'bIs'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'inspect',
      expect.objectContaining({
        action: 'inspect_class',
        className: '/Game/Blueprints/Foo.Foo_C',
        detailed: true,
        includeInherited: true,
        functionFilter: 'OnPaint',
        functionFlagFilter: ['FUNC_BlueprintEvent'],
        propertyFilter: 'bIs'
      })
    );
  });

  it('defaults detailed/includeInherited to undefined when not provided', async () => {
    await handleInspectTools(
      'inspect_class',
      { action: 'inspect_class', className: 'Actor' },
      {} as never
    );
    const mockCalls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const call = mockCalls[0][2] as Record<string, unknown>;
    expect(call.detailed).toBeUndefined();
    expect(call.includeInherited).toBeUndefined();
    expect(call.functionFilter).toBeUndefined();
  });
});

describe('inspect_function routing', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      function: { name: 'OnPaint', definedIn: 'PaperBackground_C' }
    });
  });

  it('forwards className + functionName to bridge', async () => {
    await handleInspectTools(
      'inspect_function',
      { action: 'inspect_function', className: 'PaperBackground_C', functionName: 'OnPaint' },
      {} as never
    );
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'inspect',
      expect.objectContaining({
        action: 'inspect_function',
        className: 'PaperBackground_C',
        functionName: 'OnPaint'
      })
    );
  });

  it('throws when functionName missing', async () => {
    await expect(
      handleInspectTools(
        'inspect_function',
        { action: 'inspect_function', className: 'Foo' },
        {} as never
      )
    ).rejects.toThrow(/functionName/);
  });
});
