import { describe, expect, it } from 'vitest';
import { ResponseFactory } from './response-factory.js';

describe('ResponseFactory', () => {
  it('preserves native Error messages', () => {
    const response = ResponseFactory.error(new Error('Path traversal (..) is not allowed'));

    expect(response).toMatchObject({
      success: false,
      isError: true,
      message: 'Path traversal (..) is not allowed',
      data: null
    });
  });

  it('preserves structured automation error details', () => {
    const response = ResponseFactory.error({
      message: 'Bridge rejected request',
      code: 'BRIDGE_REJECTED',
      retryable: false
    });

    expect(response).toMatchObject({
      success: false,
      isError: true,
      message: 'Bridge rejected request',
      error: {
        message: 'Bridge rejected request',
        code: 'BRIDGE_REJECTED',
        retryable: false
      },
      data: null
    });
  });
});
