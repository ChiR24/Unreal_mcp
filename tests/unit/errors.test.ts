import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { McpError, McpErrorCode } from '../../src/types/errors.js';
import { mapToMcpError } from '../../src/tools/handlers/common-handlers.js';
import { ErrorHandler, ErrorType } from '../../src/utils/error-handler.js';

describe('Enhanced Error Types', () => {
  describe('McpError', () => {
    it('should create an error with correct properties', () => {
      const error = new McpError(McpErrorCode.ASSET_NOT_FOUND, 'Asset not found', {
        retryable: true,
        suggestedFixes: ['Check path']
      });

      expect(error).toBeInstanceOf(Error);
      expect(error).toBeInstanceOf(McpError);
      expect(error.message).toBe('Asset not found');
      expect(error.code).toBe(McpErrorCode.ASSET_NOT_FOUND);
      expect(error.retryable).toBe(true);
      expect(error.suggestedFixes).toEqual(['Check path']);
    });

    it('should serialize to JSON correctly', () => {
      const error = new McpError(McpErrorCode.INVALID_PARAMS, 'Bad params');
      const json = error.toJSON();
      
      expect(json).toEqual({
        name: 'McpError',
        code: McpErrorCode.INVALID_PARAMS,
        message: 'Bad params',
        retryable: false,
        recoverable: false,
        suggestedFixes: [],
        details: undefined
      });
    });
  });

  describe('mapToMcpError', () => {
    it('should return McpError as is', () => {
      const original = new McpError(McpErrorCode.INTERNAL_ERROR, 'test');
      const mapped = mapToMcpError(original);
      expect(mapped).toBe(original);
    });

    it('should map connection errors', () => {
      const err = new Error('Connection refused');
      const mapped = mapToMcpError(err);
      expect(mapped.code).toBe(McpErrorCode.UE_NOT_CONNECTED);
      expect(mapped.retryable).toBe(true);
    });

    it('should map timeout errors', () => {
      const err = new Error('Request timed out');
      const mapped = mapToMcpError(err);
      expect(mapped.code).toBe(McpErrorCode.BRIDGE_TIMEOUT);
    });

    it('should map asset errors', () => {
      const err = new Error('Asset not found: /Game/Missing');
      const mapped = mapToMcpError(err);
      expect(mapped.code).toBe(McpErrorCode.ASSET_NOT_FOUND);
    });

    it('should map unknown errors to INTERNAL_ERROR', () => {
      const err = new Error('Random explosion');
      const mapped = mapToMcpError(err);
      expect(mapped.code).toBe(McpErrorCode.INTERNAL_ERROR);
    });
  });

  describe('ErrorHandler integration', () => {
    const originalEnv = process.env.NODE_ENV;
    
    beforeAll(() => {
      process.env.NODE_ENV = 'development';
    });

    afterAll(() => {
      process.env.NODE_ENV = originalEnv;
    });

    it('should use McpError code for categorization', () => {
      const error = new McpError(McpErrorCode.ASSET_NOT_FOUND, 'Missing');
      const response = ErrorHandler.createErrorResponse(error, 'test_tool');
      
      expect(response._debug?.errorType).toBe(ErrorType.UNREAL_ENGINE);
      expect(response.code).toBe(McpErrorCode.ASSET_NOT_FOUND);
    });

    it('should include suggested fixes in response', () => {
      const error = new McpError(McpErrorCode.INVALID_PARAMS, 'Bad input', {
        suggestedFixes: ['Use positive number']
      });
      const response = ErrorHandler.createErrorResponse(error, 'test_tool');
      
      expect(response.errorDetails?.expected).toContainEqual(
        expect.objectContaining({ hint: 'Use positive number' })
      );
    });
  });
});
