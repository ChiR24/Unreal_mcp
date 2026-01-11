/**
 * Unit tests for asset-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAssetTools } from './asset-handlers.js';
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
    assetTools: {
      createFolder: vi.fn().mockResolvedValue({ success: true, path: '/Game/NewFolder' }),
      importAsset: vi.fn().mockResolvedValue({ success: true }),
      duplicateAsset: vi.fn().mockResolvedValue({ success: true }),
      renameAsset: vi.fn().mockResolvedValue({ success: true }),
      moveAsset: vi.fn().mockResolvedValue({ success: true }),
      deleteAssets: vi.fn().mockResolvedValue({ success: true }),
      generateLODs: vi.fn().mockResolvedValue({ success: true }),
      createThumbnail: vi.fn().mockResolvedValue({ success: true }),
      getMetadata: vi.fn().mockResolvedValue({ success: true, tags: {}, metadata: {} }),
      validate: vi.fn().mockResolvedValue({ success: true }),
      generateReport: vi.fn().mockResolvedValue({ success: true }),
      searchAssets: vi.fn().mockResolvedValue({ success: true, assets: [] }),
      findByTag: vi.fn().mockResolvedValue({ success: true, assets: [] }),
      getDependencies: vi.fn().mockResolvedValue({ success: true, dependencies: [] }),
      getSourceControlState: vi.fn().mockResolvedValue({ success: true }),
    },
    actorTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
    ...overrides,
  } as unknown as ITools;
}

describe('handleAssetTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS (3+) ============

  describe('success cases', () => {
    it('handles list action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        assets: [
          { path: '/Game/Asset1', name: 'Asset1' },
          { path: '/Game/Asset2', name: 'Asset2' },
        ],
        folders: [],
      });

      const result = await handleAssetTools('list', { path: '/Game' }, mockTools);

      expect(result).toHaveProperty('success', true);
      // Response wraps assets inside 'data' via ResponseFactory.success
      expect(result).toHaveProperty('data');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'list',
        expect.objectContaining({ path: '/Game' })
      );
    });

    it('handles create_folder action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('create_folder', { path: '/Game/NewFolder' }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.assetTools.createFolder).toHaveBeenCalledWith('/Game/NewFolder');
    });

    it('handles import action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('import', {
        sourcePath: 'C:/Assets/texture.png',
        destinationPath: '/Game/Textures/MyTexture',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.assetTools.importAsset).toHaveBeenCalledWith(
        expect.objectContaining({
          sourcePath: 'C:/Assets/texture.png',
          destinationPath: '/Game/Textures/MyTexture',
        })
      );
    });

    it('handles delete action with single path', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('delete', { assetPath: '/Game/OldAsset' }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.assetTools.deleteAssets).toHaveBeenCalled();
    });
  });

  // ============ AUTOMATION FAILURE TESTS (2+) ============

  describe('automation failure cases', () => {
    it('returns error when executeAutomationRequest rejects for list action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(new Error('Automation bridge not connected'));

      const result = await handleAssetTools('list', { path: '/Game' }, mockTools);

      expect(result).toHaveProperty('success', false);
      // ResponseFactory.error returns { success, message, data } - no 'error' property
      expect(result).toHaveProperty('message');
    });

    it('returns error when assetTools.createFolder fails', async () => {
      const mockTools = createMockTools();
      (mockTools.assetTools.createFolder as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Failed to create folder')
      );

      const result = await handleAssetTools('create_folder', { path: '/Game/Test' }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });

    it('returns error when set_tags automation request fails', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(new Error('Bridge timeout'));

      const result = await handleAssetTools('set_tags', {
        assetPath: '/Game/MyAsset',
        tags: ['tag1', 'tag2'],
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });
  });

  // ============ INVALID ACTION / VALIDATION TESTS (3+) ============

  describe('invalid action and validation cases', () => {
    it('falls through to default case for unknown action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'INVALID_SUBACTION',
        message: 'Unknown subaction',
      });

      const result = await handleAssetTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'INVALID_SUBACTION');
    });

    it('returns validation error for create_folder with invalid path', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('create_folder', { path: 'InvalidPath' }, mockTools);

      expect(result).toHaveProperty('success', false);
      // ResponseFactory.error passes error code as message when given a string
      expect(result).toHaveProperty('message');
      // The message contains the error code or the detailed error message
      expect((result as { message: string }).message).toBeDefined();
    });

    it('returns error when delete action has no paths', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('delete', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });

    it('returns error when duplicate action missing required params', async () => {
      const mockTools = createMockTools();

      const result = await handleAssetTools('duplicate', { sourcePath: '/Game/Source' }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('message');
    });
  });
});
