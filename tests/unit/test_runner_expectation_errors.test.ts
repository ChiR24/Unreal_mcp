import { describe, expect, it } from 'vitest';
import { evaluateExpectation } from '../test-runner.mjs';

describe('test runner specific error alternatives', () => {
  it('rejects an unrelated error when a specific error alternative is required', () => {
    const result = evaluateExpectation(
      { expected: 'error|invalid_sequence' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'ACTOR_NOT_FOUND',
          message: 'Actor not found'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('invalid_sequence');
  });

  it('does not treat 1006 inside response metadata as a connection crash', () => {
    const result = evaluateExpectation(
      { expected: 'error|asset_not_found|not found' },
      {
        isError: true,
        structuredContent: {
          requestId: 'request-1006-metadata',
          success: false,
          error: 'ASSET_NOT_FOUND',
          message: 'No assets deleted. 1 path not found.'
        }
      }
    );

    expect(result.passed).toBe(true);
  });

  it('rejects a transport crash reported only in MCP text content', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        content: [{
          type: 'text',
          text: 'WebSocket closed with code 1006 before the response completed'
        }]
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('rejects a socket hang up reported only in MCP text content', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        content: [{ type: 'text', text: 'socket hang up' }]
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('rejects standalone close code 1006 in an error field', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: '1006',
          message: 'Transport failed'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('does not treat successful content mentioning not connected as a crash', () => {
    const result = evaluateExpectation(
      { expected: 'success' },
      {
        isError: false,
        content: [{
          type: 'text',
          text: 'Success: not connected nodes were skipped'
        }]
      }
    );

    expect(result.passed).toBe(true);
  });

  it('requires an object error pattern to match the actual error code', () => {
    const result = evaluateExpectation(
      {
        expected: {
          condition: 'error',
          errorPattern: 'SECURITY_VIOLATION'
        }
      },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'MRQ_JOB_NOT_FOUND',
          message: 'Movie Render Queue job not found'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Error pattern not matched');
  });
});
