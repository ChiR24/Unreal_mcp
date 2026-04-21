import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(
    async (): Promise<Record<string, unknown>> => ({ success: true })
  ),
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  },
}));

import { handleDataTools } from './data-handlers.js';

describe('manage_data skeleton', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('throws Unsupported for unrecognised actions', async () => {
    await expect(
      handleDataTools('__not_a_real_action__', {} as never, {} as never)
    ).rejects.toThrow(/Unsupported manage_data action/);
  });
});

describe('manage_data create_data_table', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      assetPath: '/Game/Data/DT_Test',
    });
  });

  it('forwards path, name, rowStructPath to the automation bridge', async () => {
    const res = await handleDataTools(
      'create_data_table',
      {
        path: '/Game/Data',
        name: 'DT_Test',
        rowStructPath: '/Game/Data/ST_Row.ST_Row',
      } as unknown as Record<string, unknown>,
      {} as never
    );

    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/Data/DT_Test');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    expect(calls[0][1]).toBe('manage_data');
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('create_data_table');
    expect(payload.path).toBe('/Game/Data');
    expect(payload.name).toBe('DT_Test');
    expect(payload.rowStructPath).toBe('/Game/Data/ST_Row.ST_Row');
  });

  it('throws on missing rowStructPath', async () => {
    await expect(
      handleDataTools(
        'create_data_table',
        { path: '/Game', name: 'X' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/rowStructPath/);
  });

  it('throws on missing name', async () => {
    await expect(
      handleDataTools(
        'create_data_table',
        { path: '/Game', rowStructPath: '/Game/ST.ST' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/name/);
  });
});
