/**
 * Unit tests for ai-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAITools } from './ai-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((value: unknown, _paramName: string, errorMessage: string) => {
    if (typeof value !== 'string' || value.trim() === '') {
      throw new Error(errorMessage);
    }
  }),
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

describe('handleAITools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_ai_controller action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        controllerPath: '/Game/AI/Controllers/MyController',
        message: 'AI Controller created successfully',
      });

      const result = await handleAITools('create_ai_controller', {
        name: 'MyController',
        parentPath: '/Game/AI/Controllers',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('controllerPath');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_ai',
        expect.objectContaining({
          name: 'MyController',
          subAction: 'create_ai_controller',
        }),
        expect.any(String),
        expect.objectContaining({ timeoutMs: expect.any(Number) })
      );
    });

    it('handles assign_behavior_tree action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Behavior tree assigned successfully',
      });

      const result = await handleAITools('assign_behavior_tree', {
        controllerPath: '/Game/AI/Controllers/MyController',
        behaviorTreePath: '/Game/AI/BehaviorTrees/MyBT',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/AI/Controllers/MyController',
        'controllerPath',
        'Missing required parameter: controllerPath'
      );
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/AI/BehaviorTrees/MyBT',
        'behaviorTreePath',
        'Missing required parameter: behaviorTreePath'
      );
    });

    it('handles create_behavior_tree action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        behaviorTreePath: '/Game/AI/BehaviorTrees/NewBT',
        message: 'Behavior tree created successfully',
      });

      const result = await handleAITools('create_behavior_tree', {
        name: 'NewBT',
        parentPath: '/Game/AI/BehaviorTrees',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('behaviorTreePath');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'NewBT',
        'name',
        'Missing required parameter: name'
      );
    });

    it('handles create_eqs_query action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        queryPath: '/Game/AI/EQS/FindCover',
        message: 'EQS Query created successfully',
      });

      const result = await handleAITools('create_eqs_query', {
        name: 'FindCover',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('queryPath');
    });

    it('handles get_ai_info with valid path parameter', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        behaviorTree: {
          path: '/Game/AI/BehaviorTrees/MyBT',
          nodes: [],
        },
      });

      const result = await handleAITools('get_ai_info', {
        behaviorTreePath: '/Game/AI/BehaviorTrees/MyBT',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalled();
    });
  });

  // ============ VALIDATION FAILURE TESTS ============

  describe('validation failure cases', () => {
    it('throws error when create_ai_controller is missing name parameter', async () => {
      const mockTools = createMockTools();
      // Mock requireNonEmptyString to actually throw for missing value
      mockedRequireNonEmptyString.mockImplementation((value: unknown, _paramName: string, errorMessage?: string): string => {
        if (typeof value !== 'string' || value.trim() === '') {
          throw new Error(errorMessage ?? 'Value is required');
        }
        return value;
      });

      await expect(
        handleAITools('create_ai_controller', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: name');
    });

    it('throws error when assign_behavior_tree is missing controllerPath', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value: unknown, _paramName: string, errorMessage?: string): string => {
        if (typeof value !== 'string' || value.trim() === '') {
          throw new Error(errorMessage ?? 'Value is required');
        }
        return value;
      });

      await expect(
        handleAITools('assign_behavior_tree', {
          behaviorTreePath: '/Game/AI/BT/MyBT',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: controllerPath');
    });

    it('returns error when get_ai_info has no path parameters', async () => {
      const mockTools = createMockTools();

      const result = await handleAITools('get_ai_info', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'MISSING_PARAMETER');
      expect(result).toHaveProperty('message');
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for AI action: create_ai_controller')
      );

      await expect(
        handleAITools('create_ai_controller', { name: 'TestController' }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });

    it('propagates automation error response', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'ASSET_CREATION_FAILED',
        message: 'Failed to create behavior tree asset',
      });

      const result = await handleAITools('create_behavior_tree', {
        name: 'FailedBT',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
    });
  });

  // ============ UNKNOWN ACTION TESTS ============

  describe('unknown action cases', () => {
    it('returns UNKNOWN_ACTION error for invalid action', async () => {
      const mockTools = createMockTools();

      const result = await handleAITools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message', 'Unknown AI action: nonexistent_action');
    });
  });

  // ============ ADDITIONAL ACTION COVERAGE ============

  describe('additional AI actions', () => {
    it('handles add_blackboard_key with all required params', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Blackboard key added',
      });

      const result = await handleAITools('add_blackboard_key', {
        blackboardPath: '/Game/AI/Blackboards/MyBB',
        keyName: 'TargetActor',
        keyType: 'Object',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledTimes(3);
    });

    it('handles create_state_tree action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        stateTreePath: '/Game/AI/StateTrees/MyST',
      });

      const result = await handleAITools('create_state_tree', {
        name: 'MyST',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('stateTreePath');
    });

    it('handles add_ai_perception_component action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'AI Perception component added',
      });

      const result = await handleAITools('add_ai_perception_component', {
        blueprintPath: '/Game/Blueprints/BP_Enemy',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
    });

    it('handles create_smart_object_definition action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        definitionPath: '/Game/AI/SmartObjects/SOD_Bench',
      });

      const result = await handleAITools('create_smart_object_definition', {
        name: 'SOD_Bench',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('definitionPath');
    });
  });
});
