/**
 * Unit tests for media-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleMediaTools } from './media-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

// Import mocked functions for control
import { executeAutomationRequest } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);

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

describe('handleMediaTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_media_player action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        assetPath: '/Game/Media/MP_VideoPlayer',
        message: 'Media player created',
      });

      const result = await handleMediaTools('create_media_player', {
        name: 'MP_VideoPlayer',
        path: '/Game/Media',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('assetPath', '/Game/Media/MP_VideoPlayer');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'create_media_player',
          name: 'MP_VideoPlayer',
          path: '/Game/Media',
        }),
        expect.stringContaining('Automation bridge not available for create_media_player')
      );
    });

    it('handles play action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Media playback started',
      });

      const result = await handleMediaTools('play', {
        mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'play',
          mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
        }),
        expect.stringContaining('Automation bridge not available for play')
      );
    });

    it('handles create_media_texture action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        texturePath: '/Game/Media/MT_VideoTexture',
        message: 'Media texture created',
      });

      const result = await handleMediaTools('create_media_texture', {
        name: 'MT_VideoTexture',
        mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('texturePath', '/Game/Media/MT_VideoTexture');
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'create_media_texture',
        }),
        expect.any(String)
      );
    });

    it('handles seek action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        currentTime: 30.5,
        message: 'Seeked to position',
      });

      const result = await handleMediaTools('seek', {
        mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
        time: 30.5,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('currentTime', 30.5);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'seek',
          time: 30.5,
        }),
        expect.any(String)
      );
    });

    it('handles get_state action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        state: 'Playing',
        isPlaying: true,
        isPaused: false,
      });

      const result = await handleMediaTools('get_state', {
        mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('state', 'Playing');
      expect(result).toHaveProperty('isPlaying', true);
    });
  });

  // ============ AUTOMATION FAILURE TESTS ============

  describe('automation failure cases', () => {
    it('returns error when automation request rejects', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Automation bridge not available for create_media_player')
      );

      await expect(
        handleMediaTools('create_media_player', {
          name: 'MP_Test',
        }, mockTools)
      ).rejects.toThrow('Automation bridge not available');
    });

    it('returns error when bridge returns failure result', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'MEDIA_PLAYER_NOT_FOUND',
        message: 'Media player does not exist',
      });

      const result = await handleMediaTools('play', {
        mediaPlayerPath: '/Game/Media/NonExistent',
      }, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'MEDIA_PLAYER_NOT_FOUND');
    });

    it('handles bridge timeout gracefully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(
        new Error('Request timeout after 120000ms')
      );

      await expect(
        handleMediaTools('get_duration', {
          mediaPlayerPath: '/Game/Media/MP_VideoPlayer',
        }, mockTools)
      ).rejects.toThrow('Request timeout');
    });
  });

  // ============ PLAYLIST TESTS ============

  describe('playlist operations', () => {
    it('handles add_to_playlist action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        playlistLength: 3,
        message: 'Source added to playlist',
      });

      const result = await handleMediaTools('add_to_playlist', {
        playlistPath: '/Game/Media/PL_Videos',
        mediaSourcePath: '/Game/Media/FS_Video1',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('playlistLength', 3);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'add_to_playlist',
        }),
        expect.any(String)
      );
    });

    it('handles get_playlist action successfully', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        items: [
          '/Game/Media/FS_Video1',
          '/Game/Media/FS_Video2',
        ],
        count: 2,
      });

      const result = await handleMediaTools('get_playlist', {
        playlistPath: '/Game/Media/PL_Videos',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(result).toHaveProperty('count', 2);
    });
  });

  // ============ DEFAULT/PASSTHROUGH TESTS ============

  describe('default passthrough cases', () => {
    it('handles unknown action via default passthrough', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
        message: 'Custom action executed',
      });

      const result = await handleMediaTools('custom_media_action', {
        customParam: 'value',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_media',
        expect.objectContaining({
          action: 'custom_media_action',
          customParam: 'value',
        }),
        expect.stringContaining('Automation bridge not available for media action: custom_media_action')
      );
    });
  });

  // ============ PAYLOAD CONSTRUCTION TESTS ============

  describe('payload construction', () => {
    it('removes undefined values from payload', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: true,
      });

      await handleMediaTools('create_media_player', {
        name: 'MP_Test',
        path: undefined,
        description: undefined,
      }, mockTools);

      const callArgs = mockedExecuteAutomationRequest.mock.calls[0];
      const payload = callArgs[2] as Record<string, unknown>;
      
      expect(payload).toHaveProperty('name', 'MP_Test');
      expect(payload).toHaveProperty('action', 'create_media_player');
      expect(payload).not.toHaveProperty('path');
      expect(payload).not.toHaveProperty('description');
    });
  });
});
