/**
 * Unit tests for networking-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleNetworkingTools } from './networking-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((val, name, msg) => {
    if (typeof val !== 'string' || val.trim() === '') {
      throw new Error(msg);
    }
  }),
}));

import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

function createMockTools(): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn(),
    },
    actorTools: {},
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
  } as unknown as ITools;
}

describe('handleNetworkingTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('success cases', () => {
    it('handles set_property_replicated action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        propertyName: 'Health',
        replicated: true,
      });

      const result = await handleNetworkingTools('set_property_replicated', {
        blueprintPath: '/Game/Blueprints/BP_Character',
        propertyName: 'Health',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
    });

    it('handles create_rpc_function action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        functionName: 'Server_TakeDamage',
      });

      const result = await handleNetworkingTools('create_rpc_function', {
        blueprintPath: '/Game/Blueprints/BP_Character',
        functionName: 'Server_TakeDamage',
        rpcType: 'Server',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
    });

    it('handles set_owner action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
      });

      const result = await handleNetworkingTools('set_owner', {
        actorName: 'MyProjectile',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
    });
  });

  describe('validation failure cases', () => {
    it('throws error when blueprintPath is missing', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((val, name, msg) => {
        if (typeof val !== 'string' || val.trim() === '') {
          throw new Error(msg);
        }
      });

      await expect(
        handleNetworkingTools('set_property_replicated', {
          propertyName: 'Health',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: blueprintPath');
    });
  });

  describe('automation failure cases', () => {
    it('returns error when automation request fails', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available')
      );

      await expect(
        handleNetworkingTools('create_rpc_function', {
          blueprintPath: '/Game/Blueprints/BP_Test',
          functionName: 'Test',
          rpcType: 'Server',
        }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });
  });

  describe('unknown action cases', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();

      const result = await handleNetworkingTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });
  });
});
