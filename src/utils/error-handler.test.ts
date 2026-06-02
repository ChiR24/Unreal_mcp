import { describe, expect, it } from 'vitest';
import { ErrorHandler } from './error-handler.js';

describe('ErrorHandler', () => {
  it('classifies timed-out requests as timeout errors', () => {
    const response = ErrorHandler.createErrorResponse(
      new Error('Request 123 timed out after 1000ms'),
      'manage_asset'
    );

    expect(response.error).toBe('Operation timed out. Unreal Engine may be busy or unresponsive.');
    expect(response.message).toContain('Operation timed out');
    expect(response.retriable).toBe(true);
  });

  it('treats string HTTP 5xx statuses as retriable', () => {
    const response = ErrorHandler.createErrorResponse(
      { message: 'Service unavailable', response: { status: '503' } },
      'inspect'
    );

    expect(response.retriable).toBe(true);
  });

  it('ignores non-string context scopes', () => {
    const response = ErrorHandler.createErrorResponse(
      new Error('Invalid parameter'),
      'manage_asset',
      { scope: 42 }
    );

    expect(response.scope).toBe('tool-call/manage_asset');
  });
});
