/**
 * Unit tests for gameplay-primitives-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleGameplayPrimitivesTools } from './gameplay-primitives-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers module
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((value: unknown, paramName: string) => {
    if (typeof value !== 'string' || value.trim() === '') {
      throw new Error(`${paramName} is required`);
    }
    return value;
  }),
}));

// Import mocked functions for control
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

// Create mock tools object
function createMockTools(): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn(),
    },
  } as unknown as ITools;
}

describe('handleGameplayPrimitivesTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Reset mock implementation for requireNonEmptyString
    mockedRequireNonEmptyString.mockImplementation((value: unknown, paramName: string) => {
      if (typeof value !== 'string' || value.trim() === '') {
        throw new Error(`${paramName} is required`);
      }
      return value;
    });
  });

  // =========================================
  // SUCCESS PATH TESTS (5+)
  // =========================================

  describe('success cases', () => {
    it('should handle create_value_tracker successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        trackerKey: 'health',
        currentValue: 100,
        minValue: 0,
        maxValue: 100,
      });

      const result = await handleGameplayPrimitivesTools(
        'create_value_tracker',
        { actorName: 'Player1', trackerKey: 'health', initialValue: 100 },
        mockTools
      );

      expect(result).toMatchObject({ success: true, trackerKey: 'health' });
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_primitives',
        expect.objectContaining({ actorName: 'Player1', trackerKey: 'health' }),
        expect.any(String)
      );
    });

    it('should handle create_actor_state_machine successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        machineId: 'AI_StateMachine_1',
      });

      const result = await handleGameplayPrimitivesTools(
        'create_actor_state_machine',
        { actorName: 'Enemy1' },
        mockTools
      );

      expect(result).toMatchObject({ success: true, machineId: 'AI_StateMachine_1' });
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('Enemy1', 'actorName');
    });

    it('should handle create_faction successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        factionId: 'Police',
      });

      const result = await handleGameplayPrimitivesTools(
        'create_faction',
        { factionId: 'Police', displayName: 'Law Enforcement' },
        mockTools
      );

      expect(result).toMatchObject({ success: true, factionId: 'Police' });
    });

    it('should handle create_zone successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        zoneId: 'SafeZone_1',
      });

      const result = await handleGameplayPrimitivesTools(
        'create_zone',
        { zoneId: 'SafeZone_1', location: { x: 0, y: 0, z: 0 }, radius: 500 },
        mockTools
      );

      expect(result).toMatchObject({ success: true, zoneId: 'SafeZone_1' });
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('SafeZone_1', 'zoneId');
    });

    it('should handle get_world_time successfully (no required params)', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        hours: 14,
        minutes: 30,
        dayOfYear: 152,
        year: 2025,
      });

      const result = await handleGameplayPrimitivesTools(
        'get_world_time',
        {},
        mockTools
      );

      expect(result).toMatchObject({ success: true, hours: 14, minutes: 30 });
      // World time actions don't require actorName
      expect(mockedRequireNonEmptyString).not.toHaveBeenCalled();
    });

    it('should handle add_interactable_component successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        componentName: 'InteractableComponent_1',
      });

      const result = await handleGameplayPrimitivesTools(
        'add_interactable_component',
        { actorName: 'Chest_01', interactionType: 'hold', holdDuration: 1.5 },
        mockTools
      );

      expect(result).toMatchObject({ success: true, componentName: 'InteractableComponent_1' });
    });

    it('should handle create_spawner successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        spawnerName: 'EnemySpawner_1',
      });

      const result = await handleGameplayPrimitivesTools(
        'create_spawner',
        { actorName: 'SpawnPoint_01', spawnClass: '/Game/BP_Enemy' },
        mockTools
      );

      expect(result).toMatchObject({ success: true, spawnerName: 'EnemySpawner_1' });
    });
  });

  // =========================================
  // AUTOMATION FAILURE TESTS (2+)
  // =========================================

  describe('automation failure cases', () => {
    it('should propagate automation failure for create_value_tracker', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available')
      );

      await expect(
        handleGameplayPrimitivesTools(
          'create_value_tracker',
          { actorName: 'Player1', trackerKey: 'health' },
          mockTools
        )
      ).rejects.toThrow('Automation bridge not available');
    });

    it('should propagate automation failure for set_faction_relationship', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Bridge connection timeout')
      );

      await expect(
        handleGameplayPrimitivesTools(
          'set_faction_relationship',
          { factionA: 'Police', factionB: 'Criminals', relationship: 'hostile' },
          mockTools
        )
      ).rejects.toThrow('Bridge connection timeout');
    });

    it('should propagate automation failure for create_condition', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Network error')
      );

      await expect(
        handleGameplayPrimitivesTools(
          'create_condition',
          { conditionId: 'HasKey' },
          mockTools
        )
      ).rejects.toThrow('Network error');
    });
  });

  // =========================================
  // VALIDATION TESTS (3+)
  // =========================================

  describe('validation cases', () => {
    it('should throw error when actorName is missing for create_value_tracker', async () => {
      const mockTools = createMockTools();

      await expect(
        handleGameplayPrimitivesTools(
          'create_value_tracker',
          { trackerKey: 'health' },
          mockTools
        )
      ).rejects.toThrow('actorName is required');
    });

    it('should throw error when trackerKey is missing for modify_value', async () => {
      const mockTools = createMockTools();

      await expect(
        handleGameplayPrimitivesTools(
          'modify_value',
          { actorName: 'Player1' },
          mockTools
        )
      ).rejects.toThrow('trackerKey is required');
    });

    it('should throw error when factionA is missing for set_faction_relationship', async () => {
      const mockTools = createMockTools();

      await expect(
        handleGameplayPrimitivesTools(
          'set_faction_relationship',
          { factionB: 'Criminals' },
          mockTools
        )
      ).rejects.toThrow('factionA is required');
    });

    it('should throw error when zoneId is missing for create_zone', async () => {
      const mockTools = createMockTools();

      await expect(
        handleGameplayPrimitivesTools(
          'create_zone',
          { location: { x: 0, y: 0, z: 0 } },
          mockTools
        )
      ).rejects.toThrow('zoneId is required');
    });

    it('should throw error when conditionId is missing for evaluate_condition', async () => {
      const mockTools = createMockTools();

      await expect(
        handleGameplayPrimitivesTools(
          'evaluate_condition',
          {},
          mockTools
        )
      ).rejects.toThrow('conditionId is required');
    });
  });

  // =========================================
  // FALLTHROUGH / UNKNOWN ACTION TESTS (2+)
  // =========================================

  describe('unknown action cases', () => {
    it('should pass unknown actions to bridge via default case', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'NOT_IMPLEMENTED',
        message: 'Action not implemented in C++',
      });

      const result = await handleGameplayPrimitivesTools(
        'some_future_action',
        { someParam: 'value' },
        mockTools
      );

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_primitives',
        { someParam: 'value' },
        expect.stringContaining('some_future_action')
      );
      expect(result).toMatchObject({ success: false, error: 'NOT_IMPLEMENTED' });
    });

    it('should handle empty action string via default case', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'UNKNOWN_ACTION',
      });

      const result = await handleGameplayPrimitivesTools('', {}, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalled();
      expect(result).toMatchObject({ success: false });
    });
  });
});
