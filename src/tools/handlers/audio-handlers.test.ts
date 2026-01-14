/**
 * Unit tests for audio-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAudioTools } from './audio-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((value: unknown, _p: string, msg?: string): string => {
    if (typeof value !== 'string' || !value.trim()) throw new Error(msg);
    return value;
  }),
}));

// Import mocked functions for control
import { requireNonEmptyString } from './common-handlers.js';
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

// Create mock tools object
function createMockTools(overrides: Partial<ITools> = {}): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
    audioTools: {
      createSoundCue: vi.fn().mockResolvedValue({ success: true, path: '/Game/Audio/NewCue' }),
      playSoundAtLocation: vi.fn().mockResolvedValue({ success: true }),
      playSound2D: vi.fn().mockResolvedValue({ success: true }),
      createAudioComponent: vi.fn().mockResolvedValue({ success: true, componentName: 'AudioComp' }),
      setSoundAttenuation: vi.fn().mockResolvedValue({ success: true }),
      createSoundClass: vi.fn().mockResolvedValue({ success: true }),
      createSoundMix: vi.fn().mockResolvedValue({ success: true }),
      pushSoundMix: vi.fn().mockResolvedValue({ success: true }),
      popSoundMix: vi.fn().mockResolvedValue({ success: true }),
      createAmbientSound: vi.fn().mockResolvedValue({ success: true }),
      createReverbZone: vi.fn().mockResolvedValue({ success: true }),
      enableAudioAnalysis: vi.fn().mockResolvedValue({ success: true }),
      fadeSound: vi.fn().mockResolvedValue({ success: true }),
      setDopplerEffect: vi.fn().mockResolvedValue({ success: true }),
      setAudioOcclusion: vi.fn().mockResolvedValue({ success: true }),
    },
    assetTools: {},
    actorTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
    ...overrides,
  } as unknown as ITools;
}

describe('handleAudioTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // ============ SUCCESS TESTS ============

  describe('success cases', () => {
    it('handles create_sound_cue action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('create_sound_cue', {
        name: 'TestCue',
        soundPath: '/Game/Audio/TestWave',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.audioTools.createSoundCue).toHaveBeenCalledWith(
        expect.objectContaining({
          name: 'TestCue',
          wavePath: '/Game/Audio/TestWave',
        })
      );
    });

    it('handles play_sound_at_location action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('play_sound_at_location', {
        soundPath: '/Game/Audio/TestSound',
        location: { x: 100, y: 200, z: 300 },
        volume: 0.8,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.audioTools.playSoundAtLocation).toHaveBeenCalledWith(
        expect.objectContaining({
          soundPath: '/Game/Audio/TestSound',
          location: [100, 200, 300],
          volume: 0.8,
        })
      );
    });

    it('handles play_sound_2d action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('play_sound_2d', {
        soundPath: '/Game/Audio/UISound',
        volume: 1.0,
        pitch: 1.2,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.audioTools.playSound2D).toHaveBeenCalledWith(
        expect.objectContaining({
          soundPath: '/Game/Audio/UISound',
          volume: 1.0,
          pitch: 1.2,
        })
      );
    });

    it('handles create_audio_component action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('create_audio_component', {
        actorName: 'MyActor',
        componentName: 'AudioComp',
        soundPath: '/Game/Audio/AmbientSound',
        autoPlay: true,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.audioTools.createAudioComponent).toHaveBeenCalledWith(
        expect.objectContaining({
          actorName: 'MyActor',
          componentName: 'AudioComp',
          soundPath: '/Game/Audio/AmbientSound',
          autoPlay: true,
        })
      );
    });

    it('handles set_doppler_effect action successfully', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('set_doppler_effect', {
        enabled: true,
        scale: 2.0,
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.audioTools.setDopplerEffect).toHaveBeenCalledWith(
        expect.objectContaining({
          enabled: true,
          scale: 2.0,
        })
      );
    });

    it('handles MetaSounds action via bridge passthrough', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('create_metasound', {
        name: 'TestMetaSound',
        savePath: '/Game/Audio/MetaSounds',
      }, mockTools);

      expect(result).toHaveProperty('success', true);
      expect(mockTools.automationBridge?.sendAutomationRequest).toHaveBeenCalledWith(
        'manage_audio',
        expect.objectContaining({
          name: 'TestMetaSound',
          action: 'create_metasound',
        }),
        expect.any(Object)
      );
    });
  });

  // ============ FAILURE TESTS ============

  describe('failure cases', () => {
    it('throws error when create_sound_cue missing name', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value: unknown, _p: string, msg?: string): string => {
        if (typeof value !== 'string' || !value.trim()) throw new Error(msg);
        return value;
      });

      await expect(
        handleAudioTools('create_sound_cue', { soundPath: '/Game/Audio/Test' }, mockTools)
      ).rejects.toThrow('Missing required parameter: name');
    });

    it('throws error when play_sound_at_location missing soundPath', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value: unknown, _p: string, msg?: string): string => {
        if (typeof value !== 'string' || !value.trim()) throw new Error(msg);
        return value;
      });

      await expect(
        handleAudioTools('play_sound_at_location', { location: { x: 0, y: 0, z: 0 } }, mockTools)
      ).rejects.toThrow('Missing required parameter: soundPath');
    });

    it('throws error when create_audio_component missing actorName', async () => {
      const mockTools = createMockTools();
      mockedRequireNonEmptyString.mockImplementation((value: unknown, _p: string, msg?: string): string => {
        if (typeof value !== 'string' || !value.trim()) throw new Error(msg);
        return value;
      });

      await expect(
        handleAudioTools('create_audio_component', {
          componentName: 'AudioComp',
          soundPath: '/Game/Audio/Sound',
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: actorName');
    });

    it('returns error when audioTools.createSoundCue rejects', async () => {
      const mockTools = createMockTools();
      (mockTools.audioTools.createSoundCue as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Failed to create sound cue')
      );

      await expect(
        handleAudioTools('create_sound_cue', {
          name: 'TestCue',
          soundPath: '/Game/Audio/Test',
        }, mockTools)
      ).rejects.toThrow('Failed to create sound cue');
    });

    it('returns error when bridge passthrough fails', async () => {
      const mockTools = createMockTools();
      (mockTools.automationBridge?.sendAutomationRequest as ReturnType<typeof vi.fn>).mockRejectedValue(
        new Error('Bridge unavailable')
      );

      await expect(
        handleAudioTools('spawn_sound_at_location', {
          soundPath: '/Game/Audio/Test',
          location: { x: 0, y: 0, z: 0 },
        }, mockTools)
      ).rejects.toThrow('Bridge unavailable');
    });
  });

  // ============ UNKNOWN ACTION TEST ============

  describe('unknown action', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();

      const result = await handleAudioTools('nonexistent_action', {}, mockTools);

      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
      expect(result).toHaveProperty('message');
      expect((result as { message: string }).message).toContain('nonexistent_action');
    });
  });
});
