/**
 * Unit tests for physics-destruction-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handlePhysicsDestructionTools } from './physics-destruction-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock executeAutomationRequest
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

// Import mocked function for control
import { executeAutomationRequest } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);

// Create mock tools object
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

describe('handlePhysicsDestructionTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('success cases', () => {
    it('handles create_geometry_collection action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        assetPath: '/Game/GeometryCollections/GC_Test',
      });

      const result = await handlePhysicsDestructionTools(
        'create_geometry_collection',
        { asset_name: 'GC_Test', source_mesh: '/Game/Meshes/SM_Wall' },
        mockTools
      );

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_physics_destruction',
        expect.objectContaining({ action_type: 'create_geometry_collection' }),
        expect.any(String)
      );
    });

    it('handles fracture_uniform action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        fragmentCount: 25,
      });

      const result = await handlePhysicsDestructionTools(
        'fracture_uniform',
        { asset_path: '/Game/GC/GC_Wall', fragment_count: 25 },
        mockTools
      );

      expect(result).toHaveProperty('success', true);
    });

    it('handles create_field_system_actor action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        actorName: 'FieldSystem_0',
      });

      const result = await handlePhysicsDestructionTools(
        'create_field_system_actor',
        { actor_name: 'MyFieldSystem' },
        mockTools
      );

      expect(result).toHaveProperty('success', true);
    });
  });

  describe('failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available')
      );

      await expect(
        handlePhysicsDestructionTools('fracture_uniform', {}, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });
  });

  describe('unknown action cases', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();

      const result = await handlePhysicsDestructionTools(
        'nonexistent_physics_action',
        {},
        mockTools
      );

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error');
    });
  });
});
