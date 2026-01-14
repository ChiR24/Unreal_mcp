/**
 * Unit tests for animation-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAnimationTools } from './animation-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock executeAutomationRequest
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

// Import mocked function for control
import { executeAutomationRequest } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);

// Create mock tools object
function createMockTools(overrides: Partial<ITools> = {}): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn(),
    },
    actorTools: {
      spawn: vi.fn().mockResolvedValue({ success: true }),
      delete: vi.fn().mockResolvedValue({ success: true }),
      getComponents: vi.fn().mockResolvedValue({ success: true, components: [] }),
      setTransform: vi.fn().mockResolvedValue({ success: true }),
      getTransform: vi.fn().mockResolvedValue({ success: true }),
    },
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
    animationTools: {
      createBlendSpace: vi.fn().mockResolvedValue({ success: true }),
      createStateMachine: vi.fn().mockResolvedValue({ success: true }),
      setupIK: vi.fn().mockResolvedValue({ success: true }),
      createProceduralAnim: vi.fn().mockResolvedValue({ success: true }),
      createBlendTree: vi.fn().mockResolvedValue({ success: true }),
      cleanup: vi.fn().mockResolvedValue({ success: true }),
      createAnimationAsset: vi.fn().mockResolvedValue({ success: true }),
      addNotify: vi.fn().mockResolvedValue({ success: true }),
    },
    physicsTools: {
      configureVehicle: vi.fn().mockResolvedValue({ success: true }),
      setupPhysicsSimulation: vi.fn().mockResolvedValue({ success: true }),
    },
    ...overrides,
  } as unknown as ITools;
}

describe('handleAnimationTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_animation_blueprint action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        blueprintPath: '/Game/Animations/ABP_Character',
        message: 'Animation blueprint created successfully',
      });

      const result = await handleAnimationTools('create_animation_blueprint', {
        name: 'ABP_Character',
        skeletonPath: '/Game/Characters/Skeleton',
        savePath: '/Game/Animations',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_animation_authoring',
        expect.objectContaining({
          subAction: 'create_anim_blueprint',
          name: 'ABP_Character',
          skeletonPath: '/Game/Characters/Skeleton',
          savePath: '/Game/Animations',
        }),
        'Automation bridge not available for animation blueprint creation'
      );
    });

    it('handles play_anim_montage action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Montage started playing',
      });

      const result = await handleAnimationTools('play_anim_montage', {
        actorName: 'Character_1',
        montagePath: '/Game/Animations/Attack_Montage',
        playRate: 1.0,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'play_anim_montage',
        expect.objectContaining({
          actorName: 'Character_1',
          montagePath: '/Game/Animations/Attack_Montage',
          playRate: 1.0,
        }),
        'Automation bridge not available for montage playback'
      );
    });

    it('handles setup_ragdoll action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Ragdoll physics enabled',
      });

      const result = await handleAnimationTools('setup_ragdoll', {
        actorName: 'Character_1',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'setup_ragdoll',
        expect.objectContaining({
          actorName: 'Character_1',
        }),
        'Automation bridge not available for ragdoll setup'
      );
    });

    it('handles create_blend_space action via animationTools', async () => {
      const mockTools = createMockTools();
      (mockTools.animationTools.createBlendSpace as ReturnType<typeof vi.fn>).mockResolvedValue({
        success: true,
        assetPath: '/Game/Animations/BS_Locomotion',
      });

      const result = await handleAnimationTools('create_blend_space', {
        name: 'BS_Locomotion',
        path: '/Game/Animations',
        skeletonPath: '/Game/Characters/Skeleton',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.animationTools.createBlendSpace).toHaveBeenCalled();
    });
  });

  // ============ FAILURE TESTS ============

  describe('failure cases', () => {
    it('returns error when automation request rejects for create_animation_blueprint', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for animation blueprint creation')
      );

      await expect(handleAnimationTools('create_animation_blueprint', {
        name: 'ABP_Test',
        skeletonPath: '/Game/Skeleton',
      }, mockTools)).rejects.toThrow('Automation bridge not available');
    });

    it('returns actor not found error for play_anim_montage', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        result: {
          error: 'ACTOR_NOT_FOUND',
          message: 'Actor not found',
        },
      });

      const result = await handleAnimationTools('play_anim_montage', {
        actorName: 'NonExistent_Actor',
        montagePath: '/Game/Animations/Montage',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'ACTOR_NOT_FOUND');
    });

    it('returns actor not found error for setup_ragdoll', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        message: 'Actor not found; no ragdoll applied',
      });

      const result = await handleAnimationTools('setup_ragdoll', {
        actorName: 'NonExistent_Actor',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'ACTOR_NOT_FOUND');
    });
  });

  // ============ VALIDATION TESTS ============

  describe('validation cases', () => {
    it('handles setup_physics_simulation with missing required parameters', async () => {
      const mockTools = createMockTools();

      // No meshPath, skeletonPath, or actorName provided
      const result = await handleAnimationTools('setup_physics_simulation', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'INVALID_ARGUMENT');
      expect(result).toHaveProperty('message');
      const message = (result as { message?: string }).message ?? '';
      expect(message).toContain('meshPath');
    });

    it('handles play_anim_montage with playRate 0 and missing actorName as no-op', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        result: {
          error: 'INVALID_ARGUMENT',
          message: 'actorName required',
        },
      });

      const result = await handleAnimationTools('play_anim_montage', {
        montagePath: '/Game/Animations/Montage',
        playRate: 0,
      }, mockTools);

      // When playRate is 0 and actorName is missing, it should be treated as no-op
      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('noOp', true);
    });

    it('falls through to default handler for unknown action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Action processed',
      });

      await handleAnimationTools('unknown_animation_action', {
        someParam: 'value',
      }, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'animation_physics',
        { someParam: 'value' },
        'Automation bridge not available for animation/physics operations'
      );
    });
  });

  // ============ EDGE CASES ============

  describe('edge cases', () => {
    it('auto-resolves meshPath from actorName for setup_ragdoll', async () => {
      const mockTools = createMockTools();
      (mockTools.actorTools.getComponents as ReturnType<typeof vi.fn>).mockResolvedValue({
        success: true,
        components: [
          {
            type: 'SkeletalMeshComponent',
            className: 'SkeletalMeshComponent',
            path: '/Game/Characters/SK_Character',
          },
        ],
      });
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Ragdoll applied',
      });

      const result = await handleAnimationTools('setup_ragdoll', {
        actorName: 'Character_1',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.actorTools.getComponents).toHaveBeenCalledWith('Character_1');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'setup_ragdoll',
        expect.objectContaining({
          actorName: 'Character_1',
          meshPath: '/Game/Characters/SK_Character',
        }),
        expect.any(String)
      );
    });

    it('handles chaos physics actions with prefix removal', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Geometry collection created',
      });

      await handleAnimationTools('chaos_create_geometry_collection', {
        name: 'GC_Destruction',
        staticMeshPath: '/Game/Meshes/Wall',
      }, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_physics_destruction',
        expect.objectContaining({
          action_type: 'create_geometry_collection',
        }),
        'Automation bridge not available for Chaos Physics operations'
      );
    });
  });
});
