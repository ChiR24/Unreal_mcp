/**
 * Unit tests for inventory-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleInventoryTools } from './inventory-handlers.js';
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

describe('handleInventoryTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Default: requireNonEmptyString returns the input string (valid input)
    mockedRequireNonEmptyString.mockImplementation((value) => value as string);
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_item_data_asset action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        assetPath: '/Game/Items/DA_Sword',
        message: 'Item data asset created',
      });

      const result = await handleInventoryTools('create_item_data_asset', {
        name: 'DA_Sword',
        folder: '/Game/Items',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('assetPath', '/Game/Items/DA_Sword');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'DA_Sword',
        'name',
        'Missing required parameter: name'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_inventory',
        expect.objectContaining({
          name: 'DA_Sword',
          subAction: 'create_item_data_asset',
        }),
        expect.stringContaining('Automation bridge not available'),
        expect.any(Object)
      );
    });

    it('handles create_inventory_component action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        componentName: 'InventoryComponent',
        message: 'Inventory component added',
      });

      const result = await handleInventoryTools('create_inventory_component', {
        blueprintPath: '/Game/Characters/BP_Hero',
        slotCount: 20,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/Characters/BP_Hero',
        'blueprintPath',
        'Missing required parameter: blueprintPath'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalled();
    });

    it('handles create_loot_table action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        lootTablePath: '/Game/Loot/LT_CommonDrops',
        message: 'Loot table created',
      });

      const result = await handleInventoryTools('create_loot_table', {
        name: 'LT_CommonDrops',
        folder: '/Game/Loot',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('lootTablePath', '/Game/Loot/LT_CommonDrops');
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'LT_CommonDrops',
        'name',
        'Missing required parameter: name'
      );
    });

    it('handles create_crafting_recipe action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        recipePath: '/Game/Crafting/Recipe_IronSword',
        message: 'Crafting recipe created',
      });

      const result = await handleInventoryTools('create_crafting_recipe', {
        name: 'Recipe_IronSword',
        outputItemPath: '/Game/Items/DA_IronSword',
        ingredients: [{ itemPath: '/Game/Items/DA_Iron', quantity: 3 }],
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        'Recipe_IronSword',
        'name',
        'Missing required parameter: name'
      );
      expect(mockedRequireNonEmptyString).toHaveBeenCalledWith(
        '/Game/Items/DA_IronSword',
        'outputItemPath',
        'Missing required parameter: outputItemPath'
      );
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_inventory',
        expect.objectContaining({
          subAction: 'create_crafting_recipe',
        }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('handles get_inventory_info action successfully with blueprintPath', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        inventoryData: { slots: 20, items: [] },
      });

      const result = await handleInventoryTools('get_inventory_info', {
        blueprintPath: '/Game/Characters/BP_Hero',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_inventory',
        expect.objectContaining({
          subAction: 'get_inventory_info',
        }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  // ============ VALIDATION FAILURE TESTS ============

  describe('validation failure cases', () => {
    it('throws error when name is missing for create_item_data_asset', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'name' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleInventoryTools('create_item_data_asset', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: name');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('throws error when blueprintPath is missing for create_inventory_component', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value, paramName, message): string => {
        if (paramName === 'blueprintPath' && !value) {
          throw new Error(message as string);
        }
        return value as string;
      });

      await expect(
        handleInventoryTools('create_inventory_component', {
          slotCount: 20,
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: blueprintPath');

      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('returns error when no path parameter is provided for get_inventory_info', async () => {
      const mockTools = createMockTools();

      const result = await handleInventoryTools('get_inventory_info', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'MISSING_PARAMETER');
      expect(result).toHaveProperty('message', expect.stringContaining('At least one path parameter is required'));
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for inventory action: create_item_data_asset')
      );

      await expect(
        handleInventoryTools('create_item_data_asset', {
          name: 'DA_Test',
        }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });

    it('returns error when bridge returns failure result', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'ASSET_NOT_FOUND',
        message: 'Item data asset does not exist',
      });

      const result = await handleInventoryTools('set_item_properties', {
        itemPath: '/Game/Items/NonExistent',
        displayName: 'Test Item',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'ASSET_NOT_FOUND');
    });
  });

  // ============ UNKNOWN ACTION TESTS ============

  describe('unknown action cases', () => {
    it('returns UNKNOWN_ACTION error for unrecognized action', async () => {
      const mockTools = createMockTools();

      const result = await handleInventoryTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message', 'Unknown inventory action: nonexistent_action');
      expect(mockedExecuteAutomationRequest).not.toHaveBeenCalled();
    });

    it('returns UNKNOWN_ACTION for empty action string', async () => {
      const mockTools = createMockTools();

      const result = await handleInventoryTools('', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });
  });
});
