import { describe, expect, it, vi } from 'vitest';
import { pollPCGRead } from '../test-runner-pcg-wait.mjs';
import { evaluateExpectation } from '../test-runner.mjs';
import { selectCaptureValues } from '../test-runner-response-utils.mjs';

const idleRead = {
  toolName: 'manage_pcg', expected: 'success', arguments: { action: 'get_pcg_component_results' },
  assertions: [{ path: 'structuredContent.result.generating', equals: false }],
  pollUntil: { timeoutMs: 500, intervalMs: 50 },
};

describe('bounded PCG integration read polling', () => {
  it('accepts a successful read of an explicitly retained PCG timeout without masking request failures', () => {
    const testCase = { ...idleRead, arguments: { action: 'get_pcg_operation_status' } };
    const data = { state: 'timed_out', operationErrorCode: 'PCG_OPERATION_TIMEOUT', message: 'Completion timeout; native work may continue.' };
    const response = { isError: false, structuredContent: { success: true, data }, contentText: JSON.stringify(data) };
    expect(evaluateExpectation(testCase, response).passed).toBe(true);
    expect(evaluateExpectation({ ...testCase, arguments: { action: 'generate_pcg_graph' } }, response).passed).toBe(false);
    expect(evaluateExpectation(testCase, { ...response, isError: true, structuredContent: { success: false, data } }).passed).toBe(false);
    expect(evaluateExpectation(testCase, { ...response, structuredContent: { success: true, data: { ...data, state: 'running' } } }).passed).toBe(false);
  });
  it('captures the same retained timeout identity from WS and native error envelopes', () => {
    const captures = [{ key: 'operation', fromField: 'result.result.operationId' },
      { key: 'operation', fromField: 'typedError.unrealDetail.operationId' }];
    for (const response of [
      { result: { errorCode: 'PCG_OPERATION_TIMEOUT', result: { operationId: 'owned-operation' } } },
      { typedError: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', unrealDetail: { operationId: 'owned-operation' } } },
    ]) {
      expect(selectCaptureValues(response, captures)).toEqual([{ key: 'operation', value: 'owned-operation' }]);
    }
  });
  it('does not retry ordinary cases', async () => {
    const call = vi.fn().mockResolvedValue('response');
    expect(await pollPCGRead({}, call, () => false)).toBe('response');
    expect(call).toHaveBeenCalledTimes(1);
  });
  it('waits for actual readiness without repeating mutations', async () => {
    const call = vi.fn().mockResolvedValueOnce(false).mockResolvedValueOnce(true);
    expect(await pollPCGRead(idleRead, call, (value: boolean) => value)).toBe(true);
    expect(call).toHaveBeenCalledTimes(2);
  });
  it('refuses mutation polling and invalid budgets before calling the tool', async () => {
    const call = vi.fn();
    for (const testCase of [
      { ...idleRead, arguments: { action: 'generate_pcg_graph' } },
      { ...idleRead, toolName: 'system_control' },
      { ...idleRead, assertions: [] },
      { ...idleRead, pollUntil: { timeoutMs: Infinity } },
      { ...idleRead, pollUntil: { timeoutMs: 31000 } },
    ]) {
      await expect(pollPCGRead(testCase, call, () => true)).rejects.toThrow(/bounded PCG/);
    }
    expect(call).not.toHaveBeenCalled();
  });
  it('reports a deadline rather than treating silence as native idle', async () => {
    await expect(pollPCGRead({ ...idleRead, pollUntil: { timeoutMs: 50, intervalMs: 50 } },
      async () => false, () => false)).rejects.toThrow(/native work was not cancelled/);
  });
  it('does not send a final near-zero gateway timeout at the polling deadline', async () => {
    const budgets: number[] = [];
    await expect(pollPCGRead({ ...idleRead, pollUntil: { timeoutMs: 100, intervalMs: 70 } },
      async (budget: number) => { budgets.push(budget); return false; }, () => false)).rejects.toThrow(/polling deadline/);
    expect(budgets).toHaveLength(1);
    expect(budgets[0]).toBeGreaterThan(50);
  });
  it('does not hide a transport exception behind retries', async () => {
    const call = vi.fn().mockRejectedValue(new Error('transport disconnected'));
    await expect(pollPCGRead(idleRead, call, () => false)).rejects.toThrow('transport disconnected');
    expect(call).toHaveBeenCalledTimes(1);
  });
});
