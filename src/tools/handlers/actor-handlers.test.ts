/**
 * Unit tests for actor-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleActorTools } from './actor-handlers.js';
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
      spawn: vi.fn().mockResolvedValue({ success: true, actorName: 'SpawnedActor_1' }),
      delete: vi.fn().mockResolvedValue({ success: true }),
      applyForce: vi.fn().mockResolvedValue({ success: true }),
      setTransform: vi.fn().mockResolvedValue({ success: true }),
      getTransform: vi.fn().mockResolvedValue({ success: true, location: { x: 0, y: 0, z: 0 } }),
      duplicate: vi.fn().mockResolvedValue({ success: true }),
      attach: vi.fn().mockResolvedValue({ success: true }),
      detach: vi.fn().mockResolvedValue({ success: true }),
      addTag: vi.fn().mockResolvedValue({ success: true }),
      removeTag: vi.fn().mockResolvedValue({ success: true }),
      findByTag: vi.fn().mockResolvedValue({ success: true, actors: [] }),
      deleteByTag: vi.fn().mockResolvedValue({ success: true }),
      spawnBlueprint: vi.fn().mockResolvedValue({ success: true, actorName: 'BP_Actor_1' }),
      listActors: vi.fn().mockResolvedValue({ success: true, actors: [] }),
      findByName: vi.fn().mockResolvedValue({ success: true, actors: [] }),
      getComponents: vi.fn().mockResolvedValue({ success: true, components: [] }),
      setComponentProperties: vi.fn().mockResolvedValue({ success: true }),
    },
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
    ...overrides,
  } as unknown as ITools;
}

describe('handleActorTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS (3+) ============

  describe('success cases', () => {
    it('handles spawn action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleActorTools('spawn', {
        classPath: 'StaticMeshActor',
        actorName: 'TestActor',
        location: { x: 100, y: 200, z: 50 },
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('actorName', 'SpawnedActor_1');
      expect(mockTools.actorTools.spawn).toHaveBeenCalled();
    });

    it('handles delete action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleActorTools('delete', {
        actorName: 'ActorToDelete',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.actorTools.delete).toHaveBeenCalledWith({ actorName: 'ActorToDelete' });
    });

    it('handles apply_force action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleActorTools('apply_force', {
        actorName: 'PhysicsActor',
        force: { x: 1000, y: 0, z: 500 },
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.actorTools.applyForce).toHaveBeenCalledWith({
        actorName: 'PhysicsActor',
        force: { x: 1000, y: 0, z: 500 },
      });
    });

    it('handles list action successfully', async () => {
      const mockTools = createMockTools();
      (mockTools.actorTools.listActors as ReturnType<typeof vi.fn>).mockResolvedValue({
        success: true,
        actors: [
          { label: 'Actor1', name: 'Actor1' },
          { label: 'Actor2', name: 'Actor2' },
        ],
      });

      const result = await handleActorTools('list', {}, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('actors');
      expect(result).toHaveProperty('message');
    });
  });

  // ============ AUTOMATION FAILURE TESTS (2+) ============

  describe('automation failure cases', () => {
    it('returns error when spawn fails', async () => {
      const mockTools = createMockTools();
      (mockTools.actorTools.spawn as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Failed to spawn actor: class not found')
      );

      const result = await handleActorTools('spawn', {
        classPath: 'NonExistentClass',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      // ResponseFactory.error returns { success, message, data } - no 'error' property
      expect(result).toHaveProperty('message');
    });

    it('returns error when apply_force fails with physics not enabled', async () => {
      const mockTools = createMockTools();
      (mockTools.actorTools.applyForce as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Cannot apply force: PHYSICS not enabled on component')
      );
      // Also mock getComponents for the auto-enable retry path
      (mockTools.actorTools.getComponents as ReturnType<typeof vi.fn>).mockResolvedValue({
        success: true,
        components: [],
      });

      const result = await handleActorTools('apply_force', {
        actorName: 'StaticActor',
        force: { x: 100, y: 0, z: 0 },
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });

    it('returns error when delete fails', async () => {
      const mockTools = createMockTools();
      (mockTools.actorTools.delete as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Actor not found')
      );

      const result = await handleActorTools('delete', {
        actorName: 'NonExistentActor',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });
  });

  // ============ INVALID ACTION / VALIDATION TESTS (3+) ============

  describe('invalid action and validation cases', () => {
    it('falls through to bridge call for unknown action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: 'Action not recognized',
      });

      await handleActorTools('nonexistent_action', {}, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'control_actor',
        {}
      );
    });

    it('handles spawn with timeout too small', async () => {
      const mockTools = createMockTools();

      const result = await handleActorTools('spawn', {
        classPath: 'StaticMeshActor',
        timeoutMs: 50, // Too small, should error
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });

    it('handles delete action with missing actorName', async () => {
      const mockTools = createMockTools();

      // The handler normalizes args, so passing empty object should still fail
      const result = await handleActorTools('delete', {}, mockTools);

      expect(result).toHaveProperty('success', false);
    });

    it('handles attach action with missing required params', async () => {
      const mockTools = createMockTools();

      const result = await handleActorTools('attach', { childActor: 'Child' }, mockTools);

      expect(result).toHaveProperty('success', false);
    });
  });
});
