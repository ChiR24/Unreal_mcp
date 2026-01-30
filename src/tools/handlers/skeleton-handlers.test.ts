/**
 * Unit tests for skeleton-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleSkeletonTools } from './skeleton-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  normalizeLocation: vi.fn((v) => v),
}));

// Import mocked functions for control
import { executeAutomationRequest } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);

// Create mock tools object
function createMockTools(overrides: Partial<ITools> = {}): ITools {
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
    ...overrides,
  } as unknown as ITools;
}

describe('handleSkeletonTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('list_skeletal_meshes', () => {
    it('handles list_skeletal_meshes action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        meshes: [
          { path: '/Game/Meshes/SK_Mannequin', name: 'SK_Mannequin', boneCount: 65, socketCount: 5 }
        ],
        count: 1,
      });

      const result = await handleSkeletonTools('list_skeletal_meshes', {
        directory: '/Game/Meshes',
        filter: 'Mannequin'
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('count', 1);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_skeleton',
        expect.objectContaining({
          subAction: 'list_skeletal_meshes',
          directory: '/Game/Meshes',
          filter: 'Mannequin',
        }),
        expect.any(String)
      );
    });

    it('uses default directory /Game when not provided', async () => {
        const mockTools = createMockTools();
        mockedExecuteAutomationRequest.mockResolvedValue({
          success: true,
          meshes: [],
          count: 0,
        });
  
        await handleSkeletonTools('list_skeletal_meshes', {}, mockTools);
  
        expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
          mockTools,
          'manage_skeleton',
          expect.objectContaining({
            subAction: 'list_skeletal_meshes',
          }),
          expect.any(String)
        );
      });
  });

  describe('validation', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();
      const result = await handleSkeletonTools('invalid_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'INVALID_ACTION');
    });
  });
});
