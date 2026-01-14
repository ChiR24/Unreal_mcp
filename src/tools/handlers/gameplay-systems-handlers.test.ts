/**
 * Unit tests for gameplay-systems-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleGameplaySystemsTools } from './gameplay-systems-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn(),
}));

// Import mocked functions for control
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

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

describe('handleGameplaySystemsTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Default: requireNonEmptyString returns the input string (valid input)
    mockedRequireNonEmptyString.mockImplementation((value) => value as string);
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_targeting_component action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        componentName: 'TargetingComponent',
        message: 'Targeting component created',
      });

      const result = await handleGameplaySystemsTools('create_targeting_component', {
        actorName: 'BP_Player',
        maxTargetingRange: 3000,
        targetingConeAngle: 60,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('componentName', 'TargetingComponent');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('BP_Player', 'actorName');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          action: 'create_targeting_component',
          actorName: 'BP_Player',
          maxTargetingRange: 3000,
          targetingConeAngle: 60,
        })
      );
    });

    it('handles create_checkpoint_actor action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        actorName: 'Checkpoint_1',
        checkpointId: 'cp_001',
        message: 'Checkpoint actor created',
      });

      const result = await handleGameplaySystemsTools('create_checkpoint_actor', {
        actorName: 'Checkpoint_Start',
        location: { x: 100, y: 200, z: 50 },
        autoActivate: true,
        triggerRadius: 300,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          action: 'create_checkpoint_actor',
          actorName: 'Checkpoint_Start',
          autoActivate: true,
          triggerRadius: 300,
        })
      );
    });

    it('handles create_objective action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        objectiveId: 'obj_find_key',
        message: 'Objective created',
      });

      const result = await handleGameplaySystemsTools('create_objective', {
        objectiveId: 'obj_find_key',
        objectiveName: 'Find the Key',
        description: 'Locate the hidden key in the dungeon',
        objectiveType: 'Primary',
        initialState: 'Active',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('obj_find_key', 'objectiveId');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          action: 'create_objective',
          objectiveId: 'obj_find_key',
          objectiveName: 'Find the Key',
          objectiveType: 'Primary',
        })
      );
    });

    it('handles enable_photo_mode action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        enabled: true,
        message: 'Photo mode enabled',
      });

      const result = await handleGameplaySystemsTools('enable_photo_mode', {
        enabled: true,
        pauseGame: true,
        hideUI: true,
        maxCameraDistance: 800,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('enabled', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          action: 'enable_photo_mode',
          pauseGame: true,
          hideUI: true,
        })
      );
    });

    it('handles create_quest_data_asset action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        assetPath: '/Game/Quests/DA_MainQuest',
        message: 'Quest data asset created',
      });

      const result = await handleGameplaySystemsTools('create_quest_data_asset', {
        assetPath: '/Game/Quests/DA_MainQuest',
        questId: 'quest_main_01',
        questName: 'The Beginning',
        questType: 'MainQuest',
        prerequisites: [],
        rewards: [{ type: 'experience', amount: 500 }],
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('assetPath', '/Game/Quests/DA_MainQuest');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('/Game/Quests/DA_MainQuest', 'assetPath');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalled();
    });

    it('handles create_hlod_layer action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        layerName: 'HLOD_Layer0',
        message: 'HLOD layer created',
      });

      const result = await handleGameplaySystemsTools('create_hlod_layer', {
        layerName: 'HLOD_Layer0',
        cellSize: 25600,
        loadingRange: 51200,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith('HLOD_Layer0', 'layerName');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          action: 'create_hlod_layer',
          layerName: 'HLOD_Layer0',
          cellSize: 25600,
        })
      );
    });
  });

  // ============ VALIDATION FAILURE TESTS ============

  describe('validation failure cases', () => {
    it('returns error when action is missing', async () => {
      const mockTools = createMockTools();

      const result = await handleGameplaySystemsTools('', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error');
      expect((result as Record<string, unknown>).error).toContain('Missing required parameter: action');
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when actorName is missing for create_targeting_component', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName): string => {
        if (paramName === 'actorName' && !value) {
          throw new Error(`Missing required parameter: ${paramName}`);
        }
        return value as string;
      });

      await expect(
        handleGameplaySystemsTools('create_targeting_component', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: actorName');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when objectiveId is missing for create_objective', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName): string => {
        if (paramName === 'objectiveId' && !value) {
          throw new Error(`Missing required parameter: ${paramName}`);
        }
        return value as string;
      });

      await expect(
        handleGameplaySystemsTools('create_objective', {
          objectiveName: 'Test Objective',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: objectiveId');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when checkpointId is missing for save_checkpoint', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName): string => {
        if (paramName === 'checkpointId' && !value) {
          throw new Error(`Missing required parameter: ${paramName}`);
        }
        return value as string;
      });

      await expect(
        handleGameplaySystemsTools('save_checkpoint', {
          slotName: 'TestSlot',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: checkpointId');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('propagates error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for gameplay_systems action: create_world_marker')
      );

      await expect(
        handleGameplaySystemsTools('create_world_marker', {
          markerId: 'marker_001',
          location: { x: 0, y: 0, z: 0 },
        }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });

    it('returns error when bridge returns failure result', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'OBJECTIVE_NOT_FOUND',
        message: 'Objective does not exist',
      });

      const result = await handleGameplaySystemsTools('set_objective_state', {
        objectiveId: 'nonexistent_objective',
        state: 'Completed',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'OBJECTIVE_NOT_FOUND');
    });
  });

  // ============ UNKNOWN ACTION TESTS ============

  describe('unknown action cases', () => {
    it('returns error for unrecognized action', async () => {
      const mockTools = createMockTools();

      const result = await handleGameplaySystemsTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error');
      expect((result as Record<string, unknown>).error).toContain("Unknown action 'nonexistent_action'");
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('returns hint with available actions for unknown action', async () => {
      const mockTools = createMockTools();

      const result = await handleGameplaySystemsTools('invalid_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('hint');
      expect((result as Record<string, unknown>).hint).toContain('create_targeting_component');
      expect((result as Record<string, unknown>).hint).toContain('enable_photo_mode');
    });
  });

  // ============ DEFAULT VALUES TESTS ============

  describe('default values', () => {
    it('applies default values for create_targeting_component', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleGameplaySystemsTools('create_targeting_component', {
        actorName: 'BP_Test',
      }, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          componentName: 'TargetingComponent',
          maxTargetingRange: 2000.0,
          targetingConeAngle: 45.0,
          autoTargetNearest: true,
          save: false,
        })
      );
    });

    it('applies default values for enable_photo_mode', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleGameplaySystemsTools('enable_photo_mode', {}, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          enabled: true,
          pauseGame: true,
          hideUI: true,
          hidePlayer: false,
          allowCameraMovement: true,
          maxCameraDistance: 500.0,
        })
      );
    });

    it('applies default values for set_quality_level', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleGameplaySystemsTools('set_quality_level', {}, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_gameplay_systems',
        expect.objectContaining({
          overallQuality: 3,
          applyImmediately: true,
        })
      );
    });
  });
});
