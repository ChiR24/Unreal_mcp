/**
 * Unit tests for level-handlers
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleLevelTools } from './level-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';

function createMockTools(overrides: Partial<ITools> = {}): ITools {
  return {
    levelTools: {
      loadLevel: vi.fn().mockResolvedValue({ success: true }),
      saveLevel: vi.fn().mockResolvedValue({ success: true }),
      saveLevelAs: vi.fn().mockResolvedValue({ success: true }),
      createLevel: vi.fn().mockResolvedValue({ success: true }),
      addSubLevel: vi.fn().mockResolvedValue({ success: true }),
      streamLevel: vi.fn().mockResolvedValue({ success: true }),
      exportLevel: vi.fn().mockResolvedValue({ success: true }),
      importLevel: vi.fn().mockResolvedValue({ success: true }),
      listLevels: vi.fn().mockResolvedValue({ success: true, levels: [] }),
      getLevelSummary: vi.fn().mockResolvedValue({ success: true }),
      deleteLevels: vi.fn().mockResolvedValue({ success: true }),
    },
    lightingTools: {
      buildLighting: vi.fn().mockResolvedValue({ success: true }),
    },
    actorTools: {
      spawn: vi.fn().mockResolvedValue({ success: true }),
    },
    automationBridge: {
      isConnected: vi.fn().mockReturnValue(true),
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
    ...overrides,
  } as unknown as ITools;
}

describe('handleLevelTools', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = createMockTools();
  });

  describe('load action', () => {
    it('loads level with valid path', async () => {
      const result = await handleLevelTools('load', { action: 'load', levelPath: '/Game/Maps/TestLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.loadLevel).toHaveBeenCalledWith({
        levelPath: '/Game/Maps/TestLevel',
        streaming: false
      });
    });

    it('throws when levelPath is missing', async () => {
      await expect(
        handleLevelTools('load', { action: 'load' } as HandlerArgs, mockTools)
      ).rejects.toThrow('Missing required parameter: levelPath');
    });

    it('handles load_level alias', async () => {
      const result = await handleLevelTools('load_level', { action: 'load_level', levelPath: '/Game/Maps/TestLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
    });
  });

  describe('save action', () => {
    it('saves level with savePath', async () => {
      const result = await handleLevelTools('save', { action: 'save', savePath: '/Game/Maps/NewLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.saveLevelAs).toHaveBeenCalled();
    });

    it('saves current level without path', async () => {
      const result = await handleLevelTools('save', { action: 'save', levelName: 'TestLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.saveLevel).toHaveBeenCalledWith({ levelName: 'TestLevel' });
    });
  });

  describe('save_as action', () => {
    it('saves level as with savePath', async () => {
      const result = await handleLevelTools('save_as', { action: 'save_as', savePath: '/Game/Maps/NewLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.saveLevelAs).toHaveBeenCalled();
    });

    it('returns error when savePath is missing', async () => {
      const result = await handleLevelTools('save_as', { action: 'save_as' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_ARGUMENT');
    });
  });

  describe('create_level action', () => {
    it('creates level with name', async () => {
      const result = await handleLevelTools('create_level', { action: 'create_level', levelName: 'NewLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.createLevel).toHaveBeenCalled();
    });

    it('throws when levelName is missing', async () => {
      await expect(
        handleLevelTools('create_level', { action: 'create_level' } as HandlerArgs, mockTools)
      ).rejects.toThrow('Missing required parameter: levelName');
    });
  });

  describe('add_sublevel action', () => {
    it('adds sublevel with valid path', async () => {
      const result = await handleLevelTools('add_sublevel', { action: 'add_sublevel', subLevelPath: '/Game/Maps/SubLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.addSubLevel).toHaveBeenCalled();
    });

    it('throws when subLevelPath is missing', async () => {
      await expect(
        handleLevelTools('add_sublevel', { action: 'add_sublevel' } as HandlerArgs, mockTools)
      ).rejects.toThrow('Missing required parameter: subLevelPath');
    });
  });

  describe('stream action', () => {
    it('streams level with valid params', async () => {
      const result = await handleLevelTools('stream', {
        action: 'stream',
        levelPath: '/Game/Maps/StreamLevel',
        shouldBeLoaded: true
      } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
    });

    it('returns error when levelPath is missing', async () => {
      const result = await handleLevelTools('stream', {
        action: 'stream',
        shouldBeLoaded: true
      } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_ARGUMENT');
    });

    it('returns error when shouldBeLoaded is missing', async () => {
      const result = await handleLevelTools('stream', {
        action: 'stream',
        levelPath: '/Game/Maps/StreamLevel'
      } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_ARGUMENT');
    });
  });

  describe('build_lighting action', () => {
    it('builds lighting with default quality', async () => {
      const result = await handleLevelTools('build_lighting', { action: 'build_lighting' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.lightingTools.buildLighting).toHaveBeenCalledWith({
        quality: 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      });
    });
  });

  describe('list_levels action', () => {
    it('lists levels successfully', async () => {
      const result = await handleLevelTools('list_levels', { action: 'list_levels' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(mockTools.levelTools.listLevels).toHaveBeenCalled();
    });
  });

  describe('validate_level action', () => {
    it('validates level when bridge is connected', async () => {
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      vi.mocked(bridge.sendAutomationRequest).mockResolvedValue({
        success: true,
        result: { exists: true, path: '/Game/Maps/TestLevel' }
      });
      
      const result = await handleLevelTools('validate_level', { action: 'validate_level', levelPath: '/Game/Maps/TestLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(true);
      expect(result.exists).toBe(true);
    });

    it('returns error when bridge is not connected', async () => {
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      vi.mocked(bridge.isConnected).mockReturnValue(false);
      
      const result = await handleLevelTools('validate_level', { action: 'validate_level', levelPath: '/Game/Maps/TestLevel' } as HandlerArgs, mockTools);
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('BRIDGE_UNAVAILABLE');
    });
  });

  describe('default action fallthrough', () => {
    it('routes unknown actions to executeAutomationRequest', async () => {
      const _result = await handleLevelTools('unknown_action', { action: 'unknown_action' } as HandlerArgs, mockTools);
      const bridge = mockTools.automationBridge;
      if (!bridge) throw new Error('Test setup error: bridge is null');
      
      expect(bridge.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_level',
        { action: 'unknown_action' },
        {}
      );
    });
  });
});
