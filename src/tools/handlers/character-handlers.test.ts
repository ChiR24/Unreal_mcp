/**
 * Unit tests for character-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleCharacterTools } from './character-handlers.js';
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

describe('handleCharacterTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Default: requireNonEmptyString returns the input string (valid input)
    mockedRequireNonEmptyString.mockImplementation((value) => value as string);
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_character_blueprint action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        blueprintPath: '/Game/Characters/BP_NewCharacter',
        message: 'Character blueprint created',
      });

      const result = await handleCharacterTools('create_character_blueprint', {
        name: 'BP_NewCharacter',
        parentClass: 'Character',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('blueprintPath', '/Game/Characters/BP_NewCharacter');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'BP_NewCharacter',
        'name',
        'Missing required parameter: name'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_character',
        expect.objectContaining({
          name: 'BP_NewCharacter',
          subAction: 'create_character_blueprint',
        }),
        expect.stringContaining('Automation bridge not available'),
        expect.any(Object)
      );
    });

    it('handles configure_movement_speeds action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Movement speeds configured',
      });

      const result = await handleCharacterTools('configure_movement_speeds', {
        blueprintPath: '/Game/Characters/BP_Hero',
        maxWalkSpeed: 600,
        maxSprintSpeed: 1200,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/Characters/BP_Hero',
        'blueprintPath',
        'Missing required parameter: blueprintPath'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalled();
    });

    it('handles setup_mantling action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Mantling system configured',
      });

      const result = await handleCharacterTools('setup_mantling', {
        blueprintPath: '/Game/Characters/BP_Hero',
        maxMantleHeight: 200,
        animMontage: '/Game/Animations/AM_Mantle',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/Characters/BP_Hero',
        'blueprintPath',
        'Missing required parameter: blueprintPath'
      );
    });

    it('handles setup_footstep_system action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Footstep system initialized',
      });

      const result = await handleCharacterTools('setup_footstep_system', {
        blueprintPath: '/Game/Characters/BP_Hero',
        traceDistance: 50,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_character',
        expect.objectContaining({
          subAction: 'setup_footstep_system',
        }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  // ============ VALIDATION FAILURE TESTS ============

  describe('validation failure cases', () => {
    it('throws error when name is missing for create_character_blueprint', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'name' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCharacterTools('create_character_blueprint', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: name');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when blueprintPath is missing for configure_movement_speeds', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'blueprintPath' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCharacterTools('configure_movement_speeds', {
          maxWalkSpeed: 600,
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: blueprintPath');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when surfaceType is missing for map_surface_to_sound', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'surfaceType' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCharacterTools('map_surface_to_sound', {
          blueprintPath: '/Game/Characters/BP_Hero',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: surfaceType');
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for character action: create_character_blueprint')
      );

      await expect(
        handleCharacterTools('create_character_blueprint', {
          name: 'BP_Test',
        }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });

    it('returns error when bridge returns failure result', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'BLUEPRINT_NOT_FOUND',
        message: 'Blueprint does not exist',
      });

      const result = await handleCharacterTools('setup_climbing', {
        blueprintPath: '/Game/Characters/NonExistent',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'BLUEPRINT_NOT_FOUND');
    });
  });

  // ============ UNKNOWN ACTION TESTS ============

  describe('unknown action cases', () => {
    it('returns UNKNOWN_ACTION error for unrecognized action', async () => {
      const mockTools = createMockTools();

      const result = await handleCharacterTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message', 'Unknown character action: nonexistent_action');
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('returns UNKNOWN_ACTION for empty action string', async () => {
      const mockTools = createMockTools();

      const result = await handleCharacterTools('', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });
  });
});
