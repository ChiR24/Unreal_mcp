/**
 * Unit tests for combat-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleCombatTools } from './combat-handlers.js';
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

describe('handleCombatTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Default: requireNonEmptyString returns the input string (valid input)
    mockedRequireNonEmptyString.mockImplementation((value) => value as string);
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_weapon_blueprint action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        blueprintPath: '/Game/Weapons/BP_Rifle',
        message: 'Weapon blueprint created',
      });

      const result = await handleCombatTools('create_weapon_blueprint', {
        name: 'BP_Rifle',
        weaponType: 'rifle',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('blueprintPath', '/Game/Weapons/BP_Rifle');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'BP_Rifle',
        'name',
        'Missing required parameter: name'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_combat',
        expect.objectContaining({
          name: 'BP_Rifle',
          subAction: 'create_weapon_blueprint',
        }),
        expect.stringContaining('Automation bridge not available'),
        expect.any(Object)
      );
    });

    it('handles configure_hitscan action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Hitscan configured',
      });

      const result = await handleCombatTools('configure_hitscan', {
        blueprintPath: '/Game/Weapons/BP_Rifle',
        range: 10000,
        damage: 25,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/Weapons/BP_Rifle',
        'blueprintPath',
        'Missing required parameter: blueprintPath'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_combat',
        expect.objectContaining({
          blueprintPath: '/Game/Weapons/BP_Rifle',
          subAction: 'configure_hitscan',
        }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('handles create_projectile_blueprint action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        blueprintPath: '/Game/Projectiles/BP_Rocket',
        message: 'Projectile blueprint created',
      });

      const result = await handleCombatTools('create_projectile_blueprint', {
        name: 'BP_Rocket',
        projectileType: 'rocket',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('blueprintPath', '/Game/Projectiles/BP_Rocket');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'BP_Rocket',
        'name',
        'Missing required parameter: name'
      );
    });

    it('handles configure_combo_system action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Combo system configured',
      });

      const result = await handleCombatTools('configure_combo_system', {
        blueprintPath: '/Game/Characters/BP_Warrior',
        maxComboCount: 5,
        comboResetTime: 2.0,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_combat',
        expect.objectContaining({
          subAction: 'configure_combo_system',
        }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('handles create_damage_type action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        damageTypePath: '/Game/DamageTypes/DT_Fire',
        message: 'Damage type created',
      });

      const result = await handleCombatTools('create_damage_type', {
        name: 'DT_Fire',
        damageClass: 'Fire',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'DT_Fire',
        'name',
        'Missing required parameter: name'
      );
    });
  });

  // ============ VALIDATION FAILURE TESTS ============

  describe('validation failure cases', () => {
    it('throws error when name is missing for create_weapon_blueprint', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'name' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCombatTools('create_weapon_blueprint', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: name');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when blueprintPath is missing for configure_hitscan', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'blueprintPath' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCombatTools('configure_hitscan', {
          range: 10000,
          damage: 25,
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: blueprintPath');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when name is missing for create_projectile_blueprint', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'name' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleCombatTools('create_projectile_blueprint', {
          projectileType: 'rocket',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: name');
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for combat action: create_weapon_blueprint')
      );

      await expect(
        handleCombatTools('create_weapon_blueprint', {
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

      const result = await handleCombatTools('configure_weapon_mesh', {
        blueprintPath: '/Game/Weapons/NonExistent',
        meshPath: '/Game/Meshes/SM_Rifle',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'BLUEPRINT_NOT_FOUND');
    });

    it('handles bridge timeout gracefully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Request timeout after 120000ms')
      );

      await expect(
        handleCombatTools('setup_reload_system', {
          blueprintPath: '/Game/Weapons/BP_Rifle',
        }, mockTools)
      ).rejects.toThrow('Request timeout');
    });
  });

  // ============ UNKNOWN ACTION TESTS ============

  describe('unknown action cases', () => {
    it('returns UNKNOWN_ACTION error for unrecognized action', async () => {
      const mockTools = createMockTools();

      const result = await handleCombatTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message', 'Unknown combat action: nonexistent_action');
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('returns UNKNOWN_ACTION for empty action string', async () => {
      const mockTools = createMockTools();

      const result = await handleCombatTools('', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });

    it('returns UNKNOWN_ACTION for action with typo', async () => {
      const mockTools = createMockTools();

      const result = await handleCombatTools('create_weapn_blueprint', {
        name: 'BP_Test',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message', 'Unknown combat action: create_weapn_blueprint');
    });
  });
});
