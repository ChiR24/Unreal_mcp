/**
 * Unit tests for gas-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleGASTools } from './gas-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((value, _paramName, errorMessage) => {
    if (typeof value !== 'string' || value.trim() === '') {
      throw new Error(errorMessage);
    }
  }),
}));

import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

function createMockTools(): ITools {
  return {
    automationBridge: { isConnected: () => true, sendAutomationRequest: vi.fn() },
    actorTools: {},
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
  } as unknown as ITools;
}

describe('handleGASTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('success cases', () => {
    it('handles add_ability_system_component action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handleGASTools('add_ability_system_component', { blueprintPath: '/Game/BP' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });

    it('handles create_gameplay_ability action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handleGASTools('create_gameplay_ability', { name: 'GA_Test' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });

    it('handles create_gameplay_effect action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handleGASTools('create_gameplay_effect', { name: 'GE_Damage' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('validation failures', () => {
    it('throws when blueprintPath missing', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((v, _p, msg) => {
        if (typeof v !== 'string' || !v.trim()) throw new Error(msg);
        return v;
      });
      await expect(handleGASTools('add_ability_system_component', {}, mockTools))
        .rejects.toThrow('Missing required parameter: blueprintPath');
    });
  });

  describe('automation failures', () => {
    it('rejects when bridge fails', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(new Error('Bridge unavailable'));
      await expect(handleGASTools('create_attribute_set', { name: 'AS' }, mockTools))
        .rejects.toThrow('Bridge unavailable');
    });
  });

  describe('unknown action', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();
      const result = await handleGASTools('nonexistent', {}, mockTools);
      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });
  });
});
