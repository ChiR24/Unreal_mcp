import { describe, expect, it } from 'vitest';
import { evaluateExpectation, resolveCapturedValues, summarizeResponseForReport } from '../test-runner.mjs';

describe('test runner response evaluation', () => {
  it('fails success-primary expectations when structuredContent.data reports failure', () => {
    const result = evaluateExpectation(
      { expected: 'success|error' },
      {
        isError: false,
        structuredContent: {
          success: true,
          message: 'Wrapped response',
          data: {
            success: false,
            error: 'TEXTURE_ERROR',
            message: 'Failed to create render target'
          }
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('TEXTURE_ERROR');
  });

  it('allows explicit idempotent failure alternatives for success-primary expectations', () => {
    const result = evaluateExpectation(
      { expected: 'success|already exists' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'ALREADY_EXISTS',
          message: 'Folder already exists'
        }
      }
    );

    expect(result.passed).toBe(true);
    expect(result.reason).toContain('already exists');
  });

  it('allows explicit editor-state failure alternatives for success-primary expectations', () => {
    const result = evaluateExpectation(
      { expected: 'success|world partition|not available' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'WORLD_PARTITION_DISABLED',
          message: 'World Partition is not available for this map'
        }
      }
    );

    expect(result.passed).toBe(true);
    expect(result.reason).toContain('world partition');
  });

  it('rejects unrelated failures when a success-primary expectation allows a specific editor-state fallback', () => {
    const expectedNotInPie = { expected: 'success|NOT_IN_PIE' };

    const editorStateFallback = evaluateExpectation(
      expectedNotInPie,
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'NOT_IN_PIE',
          message: 'Cannot possess actor while not in PIE'
        }
      }
    );

    const missingActor = evaluateExpectation(
      expectedNotInPie,
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'ACTOR_NOT_FOUND',
          message: 'Actor not found: MCP_EditorPawn'
        }
      }
    );

    expect(editorStateFallback.passed).toBe(true);
    expect(missingActor.passed).toBe(false);
    expect(missingActor.reason).toContain('ACTOR_NOT_FOUND');
  });

  it('reports nested structuredContent.data.result failures as response failures', () => {
    const summary = summarizeResponseForReport({
      isError: false,
      structuredContent: {
        success: true,
        data: {
          result: {
            success: false,
            error: 'TEXTURE_ERROR',
            message: 'Failed to create render target'
          }
        }
      }
    });

    expect(summary.responseSuccess).toBe(false);
    expect(summary.responseIsError).toBe(true);
    expect(summary.responseError).toBe('TEXTURE_ERROR');
    expect(summary.responseMessage).toBe('Failed to create render target');
  });

  it('requires successPattern to match successful object expectations', () => {
    const matching = evaluateExpectation(
      { expected: { condition: 'success', successPattern: 'progressSent' } },
      {
        isError: false,
        structuredContent: { success: true, message: 'Progress finished', progressSent: true }
      }
    );

    const missing = evaluateExpectation(
      { expected: { condition: 'success', successPattern: 'progressSent' } },
      {
        isError: false,
        structuredContent: { success: true, message: 'Progress finished' }
      }
    );

    expect(matching.passed).toBe(true);
    expect(missing.passed).toBe(false);
    expect(missing.reason).toContain('Success pattern not matched');
  });

  it('requires errorPattern to match failed object expectations', () => {
    const matching = evaluateExpectation(
      { expected: { condition: 'error', errorPattern: 'stalled' } },
      {
        isError: true,
        structuredContent: { success: false, error: 'PROGRESS_STALLED', message: 'Progress stalled' }
      }
    );

    const missing = evaluateExpectation(
      { expected: { condition: 'error', errorPattern: 'stalled' } },
      {
        isError: true,
        structuredContent: { success: false, error: 'INVALID_ARGUMENT', message: 'Invalid input' }
      }
    );

    expect(matching.passed).toBe(true);
    expect(missing.passed).toBe(false);
    expect(missing.reason).toContain('Error pattern not matched');
  });

  it('allows success-primary object expectations to fall back only to matching errorPattern', () => {
    const success = evaluateExpectation(
      { expected: { condition: 'success', errorPattern: 'SC_DISABLED' } },
      {
        isError: false,
        structuredContent: { success: true, message: 'Source control state retrieved' }
      }
    );

    const matchingFallback = evaluateExpectation(
      { expected: { condition: 'success', errorPattern: 'SC_DISABLED' } },
      {
        isError: true,
        structuredContent: { success: false, error: 'SC_DISABLED', message: 'Source control disabled' }
      }
    );

    const missingFallback = evaluateExpectation(
      { expected: { condition: 'success', errorPattern: 'SC_DISABLED' } },
      {
        isError: true,
        structuredContent: { success: false, error: 'UE_NOT_CONNECTED', message: 'Bridge unavailable' }
      }
    );

    expect(success.passed).toBe(true);
    expect(matchingFallback.passed).toBe(true);
    expect(missingFallback.passed).toBe(false);
    expect(missingFallback.reason).toContain('Error pattern not matched');
  });

  it('resolves captured placeholders recursively through nested argument arrays', () => {
    const missingKeys: string[] = [];
    const resolved = resolveCapturedValues(
      {
        assetPath: '${captured:assetPath}',
        pins: [
          { nodeId: '${captured:sourceNodeId}', pinName: 'Out' },
          { nodeId: '${captured:missingNodeId}', pinName: 'In' }
        ],
        metadata: { owner: '${captured:owner}' },
        unchanged: 7
      },
      {
        assetPath: '/Game/MCPTest/M_Test',
        sourceNodeId: 'node-123',
        owner: 'test-runner'
      },
      (captureKey) => missingKeys.push(captureKey)
    );

    expect(resolved).toEqual({
      assetPath: '/Game/MCPTest/M_Test',
      pins: [
        { nodeId: 'node-123', pinName: 'Out' },
        { nodeId: '${captured:missingNodeId}', pinName: 'In' }
      ],
      metadata: { owner: 'test-runner' },
      unchanged: 7
    });
    expect(missingKeys).toEqual(['missingNodeId']);
  });
});
