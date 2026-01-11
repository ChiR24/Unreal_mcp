/**
 * Unit tests for common-handlers utility functions
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import {
  ensureArgsPresent,
  requireAction,
  requireNonEmptyString,
  executeAutomationRequest,
  executeAndClean,
  normalizeLocation,
  normalizeRotation
} from './common-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

describe('ensureArgsPresent', () => {
  it('passes for valid object', () => {
    expect(() => ensureArgsPresent({ action: 'test' })).not.toThrow();
  });

  it('throws for null', () => {
    expect(() => ensureArgsPresent(null)).toThrow('Invalid arguments: null or undefined');
  });

  it('throws for undefined', () => {
    expect(() => ensureArgsPresent(undefined)).toThrow('Invalid arguments: null or undefined');
  });

  it('passes for empty object', () => {
    expect(() => ensureArgsPresent({})).not.toThrow();
  });
});

describe('requireAction', () => {
  it('returns action when present', () => {
    expect(requireAction({ action: 'load' })).toBe('load');
  });

  it('throws when action is missing', () => {
    expect(() => requireAction({})).toThrow('Missing required parameter: action');
  });

  it('throws when action is empty string', () => {
    expect(() => requireAction({ action: '' })).toThrow('Missing required parameter: action');
  });

  it('throws when action is whitespace only', () => {
    expect(() => requireAction({ action: '   ' })).toThrow('Missing required parameter: action');
  });

  it('throws for null args', () => {
    expect(() => requireAction(null as unknown as Record<string, unknown>)).toThrow('Invalid arguments: null or undefined');
  });

  it('throws when action is not a string', () => {
    expect(() => requireAction({ action: 123 as unknown as string })).toThrow('Missing required parameter: action');
  });
});

describe('requireNonEmptyString', () => {
  it('returns value when valid string', () => {
    expect(requireNonEmptyString('hello', 'field')).toBe('hello');
  });

  it('throws for empty string', () => {
    expect(() => requireNonEmptyString('', 'field')).toThrow('Invalid field: must be a non-empty string');
  });

  it('throws for whitespace only', () => {
    expect(() => requireNonEmptyString('   ', 'field')).toThrow('Invalid field: must be a non-empty string');
  });

  it('throws for non-string value', () => {
    expect(() => requireNonEmptyString(123, 'field')).toThrow('Invalid field: must be a non-empty string');
  });

  it('throws for null', () => {
    expect(() => requireNonEmptyString(null, 'field')).toThrow('Invalid field: must be a non-empty string');
  });

  it('uses custom message when provided', () => {
    expect(() => requireNonEmptyString('', 'field', 'Custom error')).toThrow('Custom error');
  });
});

describe('executeAutomationRequest', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn().mockResolvedValue({ success: true })
      }
    } as unknown as ITools;
  });

  it('throws when automationBridge is null', async () => {
    const nullBridgeTools = { automationBridge: null } as unknown as ITools;
    
    await expect(
      executeAutomationRequest(nullBridgeTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Automation bridge not available');
  });

  it('throws when automationBridge is undefined', async () => {
    const undefinedBridgeTools = { automationBridge: undefined } as unknown as ITools;
    
    await expect(
      executeAutomationRequest(undefinedBridgeTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Automation bridge not available');
  });

  it('throws when sendAutomationRequest is not a function', async () => {
    const invalidBridgeTools = {
      automationBridge: { isConnected: vi.fn(), sendAutomationRequest: 'not a function' }
    } as unknown as ITools;
    
    await expect(
      executeAutomationRequest(invalidBridgeTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Automation bridge not available');
  });

  it('throws when bridge is not connected', async () => {
    vi.mocked(mockTools.automationBridge!.isConnected).mockReturnValue(false);
    
    await expect(
      executeAutomationRequest(mockTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Automation bridge is not connected to Unreal Engine');
  });

  it('returns result when bridge is connected', async () => {
    const result = await executeAutomationRequest(mockTools, 'test_tool', { action: 'test' });
    
    expect(result).toEqual({ success: true });
    expect(mockTools.automationBridge!.sendAutomationRequest).toHaveBeenCalledWith(
      'test_tool',
      { action: 'test' },
      {}
    );
  });

  it('passes options to sendAutomationRequest', async () => {
    await executeAutomationRequest(mockTools, 'test_tool', { action: 'test' }, undefined, { timeoutMs: 5000 });
    
    expect(mockTools.automationBridge!.sendAutomationRequest).toHaveBeenCalledWith(
      'test_tool',
      { action: 'test' },
      { timeoutMs: 5000 }
    );
  });

  it('uses custom error message', async () => {
    const nullBridgeTools = { automationBridge: null } as unknown as ITools;
    
    await expect(
      executeAutomationRequest(nullBridgeTools, 'test_tool', { action: 'test' }, 'Custom error message')
    ).rejects.toThrow('Custom error message');
  });
});

describe('executeAndClean', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, data: 'test' })
      }
    } as unknown as ITools;
  });

  it('returns cleaned object result', async () => {
    const result = await executeAndClean(mockTools, 'test_tool', { action: 'test' });
    
    expect(result).toEqual({ success: true, data: 'test' });
  });

  it('throws for non-object response', async () => {
    vi.mocked(mockTools.automationBridge!.sendAutomationRequest).mockResolvedValue('string result');
    
    await expect(
      executeAndClean(mockTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Bridge returned non-object for test_tool');
  });

  it('throws for array response', async () => {
    vi.mocked(mockTools.automationBridge!.sendAutomationRequest).mockResolvedValue([1, 2, 3]);
    
    await expect(
      executeAndClean(mockTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Bridge returned non-object for test_tool');
  });

  it('throws for null response', async () => {
    vi.mocked(mockTools.automationBridge!.sendAutomationRequest).mockResolvedValue(null);
    
    await expect(
      executeAndClean(mockTools, 'test_tool', { action: 'test' })
    ).rejects.toThrow('Bridge returned non-object for test_tool');
  });
});

describe('normalizeLocation', () => {
  it('returns undefined for falsy input', () => {
    expect(normalizeLocation(null)).toBeUndefined();
    expect(normalizeLocation(undefined)).toBeUndefined();
    expect(normalizeLocation('')).toBeUndefined();
    expect(normalizeLocation(0)).toBeUndefined();
  });

  it('normalizes array format', () => {
    expect(normalizeLocation([100, 200, 300])).toEqual([100, 200, 300]);
  });

  it('normalizes object format with x, y, z', () => {
    expect(normalizeLocation({ x: 100, y: 200, z: 300 })).toEqual([100, 200, 300]);
  });

  it('handles partial object (defaults to 0)', () => {
    expect(normalizeLocation({ x: 100 })).toEqual([100, 0, 0]);
    expect(normalizeLocation({ y: 200 })).toEqual([0, 200, 0]);
    expect(normalizeLocation({ z: 300 })).toEqual([0, 0, 300]);
  });

  it('converts string numbers', () => {
    expect(normalizeLocation(['100', '200', '300'])).toEqual([100, 200, 300]);
  });

  it('handles invalid array values as 0', () => {
    expect(normalizeLocation(['a', 'b', 'c'])).toEqual([0, 0, 0]);
  });

  it('returns undefined for array with less than 3 elements', () => {
    expect(normalizeLocation([100, 200])).toBeUndefined();
  });

  it('returns undefined for non-location objects', () => {
    expect(normalizeLocation({ a: 1, b: 2 })).toBeUndefined();
  });
});

describe('normalizeRotation', () => {
  it('returns undefined for falsy input', () => {
    expect(normalizeRotation(null)).toBeUndefined();
    expect(normalizeRotation(undefined)).toBeUndefined();
  });

  it('normalizes array format to object', () => {
    expect(normalizeRotation([45, 90, 0])).toEqual({ pitch: 45, yaw: 90, roll: 0 });
  });

  it('preserves object format', () => {
    expect(normalizeRotation({ pitch: 45, yaw: 90, roll: 0 })).toEqual({ pitch: 45, yaw: 90, roll: 0 });
  });

  it('handles partial object (defaults to 0)', () => {
    expect(normalizeRotation({ pitch: 45 } as { pitch: number; yaw: number; roll: number })).toEqual({ pitch: 45, yaw: 0, roll: 0 });
  });

  it('converts string numbers in array', () => {
    expect(normalizeRotation(['45', '90', '0'] as unknown as [number, number, number])).toEqual({ pitch: 45, yaw: 90, roll: 0 });
  });

  it('handles short array as object (falls through to object handler)', () => {
    // Arrays with < 3 elements fall through to object handler (arrays are objects in JS)
    // and get processed with undefined properties -> all 0s
    expect(normalizeRotation([45, 90] as unknown as [number, number, number])).toEqual({ pitch: 0, yaw: 0, roll: 0 });
  });
});
