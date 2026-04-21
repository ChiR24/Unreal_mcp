import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(
    async (): Promise<Record<string, unknown>> => ({ success: true })
  ),
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
}));

import { handleCurveTools } from './curve-handlers.js';

describe('manage_curve skeleton', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('throws Unsupported for unrecognised actions', async () => {
    await expect(
      handleCurveTools('__not_a_real_action__', {} as never, {} as never)
    ).rejects.toThrow(/Unsupported manage_curve action/);
  });
});

describe('manage_curve create_curve_float', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      assetPath: '/Game/DataTest/C_Ch5Test',
      saved: true,
    });
  });

  it('forwards path + name with subAction=create_curve_float', async () => {
    const res = await handleCurveTools(
      'create_curve_float',
      { path: '/Game/DataTest', name: 'C_Ch5Test' } as unknown as Record<string, unknown>,
      {} as never
    );

    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/DataTest/C_Ch5Test');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    expect(calls[0][1]).toBe('manage_curve');
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('create_curve_float');
    expect(payload.path).toBe('/Game/DataTest');
    expect(payload.name).toBe('C_Ch5Test');
  });

  it('throws on missing path', async () => {
    await expect(
      handleCurveTools(
        'create_curve_float',
        { name: 'X' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/path/);
  });

  it('throws on missing name', async () => {
    await expect(
      handleCurveTools(
        'create_curve_float',
        { path: '/Game/X' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/name/);
  });
});

describe('manage_curve set_curve_keys', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, keyCount: 3 });
  });

  it('forwards path + keys array with subAction=set_curve_keys', async () => {
    const keys = [
      { time: 0, value: 0, interpMode: 'Linear' },
      { time: 1, value: 1, interpMode: 'Auto' },
      { time: 2, value: 0, interpMode: 'Constant' },
    ];
    const res = await handleCurveTools(
      'set_curve_keys',
      { path: '/Game/DataTest/C_Ch5Test', keys } as unknown as Record<string, unknown>,
      {} as never
    );

    expect(res.success).toBe(true);
    expect(res.keyCount).toBe(3);
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('set_curve_keys');
    expect(payload.path).toBe('/Game/DataTest/C_Ch5Test');
    expect(Array.isArray(payload.keys)).toBe(true);
    expect((payload.keys as unknown[]).length).toBe(3);
  });

  it('throws on missing keys array', async () => {
    await expect(
      handleCurveTools(
        'set_curve_keys',
        { path: '/Game/X' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/keys/);
  });
});
