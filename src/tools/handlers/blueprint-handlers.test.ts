/**
 * Unit tests for blueprint-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleBlueprintTools } from './blueprint-handlers.js';
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
    blueprintTools: {
      createBlueprint: vi.fn().mockResolvedValue({ success: true, path: '/Game/BP_Test' }),
      waitForBlueprint: vi.fn().mockResolvedValue({ success: true }),
      addVariable: vi.fn().mockResolvedValue({ success: true }),
      setVariableMetadata: vi.fn().mockResolvedValue({ success: true }),
      removeVariable: vi.fn().mockResolvedValue({ success: true }),
      renameVariable: vi.fn().mockResolvedValue({ success: true }),
      addEvent: vi.fn().mockResolvedValue({ success: true }),
      removeEvent: vi.fn().mockResolvedValue({ success: true }),
      addFunction: vi.fn().mockResolvedValue({ success: true }),
      addComponent: vi.fn().mockResolvedValue({ success: true }),
      modifyConstructionScript: vi.fn().mockResolvedValue({ success: true }),
      setSCSComponentTransform: vi.fn().mockResolvedValue({ success: true }),
      addConstructionScript: vi.fn().mockResolvedValue({ success: true }),
      addNode: vi.fn().mockResolvedValue({ success: true }),
      addSCSComponent: vi.fn().mockResolvedValue({ success: true }),
      reparentSCSComponent: vi.fn().mockResolvedValue({ success: true }),
      setSCSComponentProperty: vi.fn().mockResolvedValue({ success: true }),
      removeSCSComponent: vi.fn().mockResolvedValue({ success: true }),
      getBlueprintSCS: vi.fn().mockResolvedValue({ success: true, components: [] }),
      setBlueprintDefault: vi.fn().mockResolvedValue({ success: true }),
      compileBlueprint: vi.fn().mockResolvedValue({ success: true }),
      probeSubobjectDataHandle: vi.fn().mockResolvedValue({ success: true }),
      getBlueprintInfo: vi.fn().mockResolvedValue({ success: true }),
    },
    actorTools: {},
    assetTools: {},
    levelTools: {},
    editorTools: {},
    ...overrides,
  } as unknown as ITools;
}

describe('handleBlueprintTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS (3+) ============

  describe('success cases', () => {
    it('handles create action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleBlueprintTools('create', {
        name: 'BP_TestActor',
        savePath: '/Game/Blueprints',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.blueprintTools.createBlueprint).toHaveBeenCalledWith(
        expect.objectContaining({
          name: 'BP_TestActor',
          savePath: '/Game/Blueprints',
        })
      );
    });

    it('handles add_variable action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleBlueprintTools('add_variable', {
        name: '/Game/Blueprints/BP_Test',
        variableName: 'Health',
        variableType: 'Float',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.blueprintTools.addVariable).toHaveBeenCalledWith(
        expect.objectContaining({
          blueprintName: '/Game/Blueprints/BP_Test',
          variableName: 'Health',
          variableType: 'Float',
        })
      );
    });

    it('handles remove_variable action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleBlueprintTools('remove_variable', {
        name: '/Game/Blueprints/BP_Test',
        variableName: 'OldVariable',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.blueprintTools.removeVariable).toHaveBeenCalledWith(
        expect.objectContaining({
          blueprintName: '/Game/Blueprints/BP_Test',
          variableName: 'OldVariable',
        })
      );
    });

    it('handles bp_ensure_exists action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleBlueprintTools('bp_ensure_exists', {
        name: '/Game/Blueprints/BP_Test',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.blueprintTools.waitForBlueprint).toHaveBeenCalledWith(
        '/Game/Blueprints/BP_Test',
        undefined
      );
    });

    it('handles compile action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleBlueprintTools('compile', {
        name: '/Game/Blueprints/BP_Test',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.blueprintTools.compileBlueprint).toHaveBeenCalledWith(
        expect.objectContaining({
          blueprintName: '/Game/Blueprints/BP_Test',
        })
      );
    });
  });

  // ============ AUTOMATION FAILURE TESTS (2+) ============

  describe('automation failure cases', () => {
    // Blueprint handlers don't wrap errors in try/catch, so exceptions bubble up
    it('throws when createBlueprint fails', async () => {
      const mockTools = createMockTools();
      (mockTools.blueprintTools.createBlueprint as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Failed to create blueprint: parent class not found')
      );

      await expect(
        handleBlueprintTools('create', {
          name: 'BP_Invalid',
          parentClass: 'NonExistentClass',
        }, mockTools)
      ).rejects.toThrow('Failed to create blueprint');
    });

    it('throws when addVariable fails', async () => {
      const mockTools = createMockTools();
      (mockTools.blueprintTools.addVariable as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Blueprint not found')
      );

      await expect(
        handleBlueprintTools('add_variable', {
          name: '/Game/NonExistent',
          variableName: 'Test',
        }, mockTools)
      ).rejects.toThrow('Blueprint not found');
    });

    it('throws when executeAutomationRequest fails for graph operations', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not connected')
      );

      await expect(
        handleBlueprintTools('connect_pins', {
          blueprintPath: '/Game/BP_Test',
          sourceNodeId: 'Node1',
          sourcePinName: 'Out',
          targetNodeId: 'Node2',
          targetPinName: 'In',
        }, mockTools)
      ).rejects.toThrow('Automation bridge not connected');
    });
  });

  // ============ INVALID ACTION / VALIDATION TESTS (3+) ============

  describe('invalid action and validation cases', () => {
    it('falls through to default case for unknown action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'INVALID_ACTION',
        message: 'Unknown blueprint action',
      });

      await handleBlueprintTools('nonexistent_action', {}, mockTools);

      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_blueprint',
        expect.any(Object),
        'Automation bridge not available for blueprint operations'
      );
    });

    it('throws error for set_metadata with missing required params', async () => {
      const mockTools = createMockTools();

      // set_metadata requires assetPath or blueprintPath or name+savePath
      await expect(
        handleBlueprintTools('set_metadata', {}, mockTools)
      ).rejects.toThrow('assetPath or blueprintPath or name+savePath required');
    });

    it('throws error for add_node CallFunction without functionName', async () => {
      const mockTools = createMockTools();

      await expect(
        handleBlueprintTools('add_node', {
          name: '/Game/BP_Test',
          nodeType: 'CallFunction',
          // functionName is missing
        }, mockTools)
      ).rejects.toThrow('CallFunction node requires functionName');
    });

    it('handles add_event returning EVENT_ALREADY_EXISTS error', async () => {
      const mockTools = createMockTools();
      (mockTools.blueprintTools.addEvent as ReturnType<typeof vi.fn>).mockResolvedValue({
        success: false,
        message: 'Event already exists',
      });

      const result = await handleBlueprintTools('add_event', {
        blueprintPath: '/Game/BP_Test',
        eventType: 'Custom',
        customEventName: 'OnDamage',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'EVENT_ALREADY_EXISTS');
    });
  });
});
