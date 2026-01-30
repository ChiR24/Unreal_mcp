/**
 * Unit tests for editor-utilities-handlers
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleEditorUtilitiesTools } from './editor-utilities-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';
import type { EditorUtilitiesArgs } from './editor-utilities-handlers.js';

function createMockTools(): ITools {
  return {
    automationBridge: {
      isConnected: vi.fn().mockReturnValue(true),
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
  } as unknown as ITools;
}

describe('handleEditorUtilitiesTools', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = createMockTools();
  });

  describe('missing action', () => {
    it('returns error when action is empty', async () => {
      const result = await handleEditorUtilitiesTools('', {} as EditorUtilitiesArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toContain('Missing required parameter: action');
    });

    it('returns error when action is whitespace', async () => {
      const result = await handleEditorUtilitiesTools('   ', {} as EditorUtilitiesArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toContain('Missing required parameter: action');
    });
  });

  describe('editor modes', () => {
    it('set_editor_mode requires modeName', async () => {
      await expect(
        handleEditorUtilitiesTools('set_editor_mode', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid modeName');
    });

    it('set_editor_mode succeeds with valid modeName', async () => {
      const _result = await handleEditorUtilitiesTools('set_editor_mode', { modeName: 'Landscape' } as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ action: 'set_editor_mode', modeName: 'Landscape' }),
        {}
      );
    });

    it('configure_editor_preferences requires category', async () => {
      await expect(
        handleEditorUtilitiesTools('configure_editor_preferences', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow(/category is required/);
    });

    it('set_grid_settings uses defaults', async () => {
      const _result = await handleEditorUtilitiesTools('set_grid_settings', {} as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ 
          action: 'set_grid_settings',
          gridSize: 10,
          rotationSnap: 15,
          scaleSnap: 0.25
        }),
        {}
      );
    });
  });

  describe('content browser', () => {
    it('navigate_to_path requires path', async () => {
      await expect(
        handleEditorUtilitiesTools('navigate_to_path', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid path');
    });

    it('navigate_to_path succeeds with valid path', async () => {
      const _result = await handleEditorUtilitiesTools('navigate_to_path', { path: '/Game/Assets' } as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ action: 'navigate_to_path', path: '/Game/Assets' }),
        {}
      );
    });

    it('create_collection requires collectionName', async () => {
      await expect(
        handleEditorUtilitiesTools('create_collection', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid collectionName');
    });

    it('add_to_collection returns error when no assets provided', async () => {
      const result = await handleEditorUtilitiesTools('add_to_collection', { collectionName: 'MyCollection' } as EditorUtilitiesArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toContain('requires assetPaths or assetPath');
    });
  });

  describe('selection', () => {
    it('select_actor requires actorName', async () => {
      await expect(
        handleEditorUtilitiesTools('select_actor', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid actorName');
    });

    it('select_actor succeeds with valid actorName', async () => {
      const _result = await handleEditorUtilitiesTools('select_actor', { actorName: 'MyActor' } as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ 
          action: 'select_actor',
          actorName: 'MyActor',
          addToSelection: false
        }),
        {}
      );
    });

    it('deselect_all requires no params', async () => {
      const _result = await handleEditorUtilitiesTools('deselect_all', {} as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ action: 'deselect_all' }),
        {}
      );
    });
  });

  describe('collision', () => {
    it('create_collision_channel requires channelName', async () => {
      await expect(
        handleEditorUtilitiesTools('create_collision_channel', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid channelName');
    });

    it('create_collision_profile requires profileName', async () => {
      await expect(
        handleEditorUtilitiesTools('create_collision_profile', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid profileName');
    });
  });

  describe('physical materials', () => {
    it('create_physical_material requires materialName', async () => {
      await expect(
        handleEditorUtilitiesTools('create_physical_material', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid materialName');
    });

    it('create_physical_material uses default values', async () => {
      const _result = await handleEditorUtilitiesTools('create_physical_material', { materialName: 'PM_Metal' } as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({
          action: 'create_physical_material',
          materialName: 'PM_Metal',
          friction: 0.7,
          restitution: 0.3,
          density: 1.0,
          save: true
        }),
        {}
      );
    });
  });

  describe('timers', () => {
    it('set_timer requires functionName and targetActor', async () => {
      await expect(
        handleEditorUtilitiesTools('set_timer', { functionName: 'MyFunc' } as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid targetActor');
    });

    it('clear_timer requires timerHandle', async () => {
      await expect(
        handleEditorUtilitiesTools('clear_timer', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid timerHandle');
    });
  });

  describe('delegates', () => {
    it('create_event_dispatcher requires blueprintPath and dispatcherName', async () => {
      await expect(
        handleEditorUtilitiesTools('create_event_dispatcher', { blueprintPath: '/Game/BP_Test' } as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid dispatcherName');
    });
  });

  describe('transactions', () => {
    it('begin_transaction requires transactionName', async () => {
      await expect(
        handleEditorUtilitiesTools('begin_transaction', {} as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Invalid transactionName');
    });

    it('undo requires no params', async () => {
      const _result = await handleEditorUtilitiesTools('undo', {} as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ action: 'undo' }),
        {}
      );
    });

    it('redo requires no params', async () => {
      const _result = await handleEditorUtilitiesTools('redo', {} as EditorUtilitiesArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_editor_utilities',
        expect.objectContaining({ action: 'redo' }),
        {}
      );
    });
  });

  describe('unknown action', () => {
    it('returns error for unknown action', async () => {
      const result = await handleEditorUtilitiesTools('unknown_action', {} as EditorUtilitiesArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toContain('Unknown manage_editor_utilities action');
    });
  });

  describe('automation failure', () => {
    it('propagates bridge errors', async () => {
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      vi.mocked(bridge.sendAutomationRequest).mockRejectedValue(new Error('Bridge error'));
      
      await expect(
        handleEditorUtilitiesTools('set_editor_mode', { modeName: 'Landscape' } as EditorUtilitiesArgs, mockTools)
      ).rejects.toThrow('Bridge error');
    });
  });
});
