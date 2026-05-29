import { beforeEach, describe, expect, it, vi } from 'vitest';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  normalizePathFields: (args: Record<string, unknown>, pathFields: readonly string[]) => {
    const result = { ...args };
    for (const field of pathFields) {
      const value = result[field];
      if (typeof value === 'string' && value.startsWith('Game/')) {
        result[field] = `/${value}`;
      }
    }
    return result;
  },
  requireNonEmptyString: (value: unknown, fieldName: string, message: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(message || `Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleAudioTools } from './audio-handlers.js';

describe('handleAudioTools audio payload mapping', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('maps create_sound_cue without injecting default modulation fields', async () => {
    await handleAudioTools(
      'create_sound_cue',
      {
        action: 'create_sound_cue',
        name: 'TestCue',
        soundPath: '/Engine/VREditor/Sounds/VR_click1',
        path: '/Game/MCPTest'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.CREATE_SOUND_CUE,
      expect.objectContaining({
        name: 'TestCue',
        packagePath: '/Game/MCPTest',
        wavePath: '/Engine/VREditor/Sounds/VR_click1',
        volume: undefined,
        pitch: undefined
      })
    );
  });

  it('normalizes creation path aliases before dispatch', async () => {
    await handleAudioTools(
      'set_sound_attenuation',
      {
        action: 'set_sound_attenuation',
        name: 'TestAttenuation',
        path: 'Game/MCPTest/Audio'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.SET_SOUND_ATTENUATION,
      expect.objectContaining({
        name: 'TestAttenuation',
        path: '/Game/MCPTest/Audio'
      })
    );
  });

  it('maps sound mix override aliases to bridge field names', async () => {
    await handleAudioTools(
      'set_sound_mix_class_override',
      {
        action: 'set_sound_mix_class_override',
        mix: '/Game/MCPTest/TestSoundMix',
        soundClass: '/Game/MCPTest/TestSoundClass',
        volume: 1
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.SET_SOUND_MIX_CLASS_OVERRIDE,
      expect.objectContaining({
        mixName: '/Game/MCPTest/TestSoundMix',
        soundClassName: '/Game/MCPTest/TestSoundClass'
      })
    );
  });

  it('maps set_base_sound_mix alias to mixName', async () => {
    await handleAudioTools(
      'set_base_sound_mix',
      {
        action: 'set_base_sound_mix',
        mix: '/Game/MCPTest/TestSoundMix'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.SET_BASE_SOUND_MIX,
      expect.objectContaining({
        mixName: '/Game/MCPTest/TestSoundMix'
      })
    );
  });

  it('clamps 2D playback volume and pitch to UE-safe ranges', async () => {
    await handleAudioTools(
      'play_sound_2d',
      {
        action: 'play_sound_2d',
        soundPath: '/Game/MCPTest/TestCue',
        volume: 12,
        pitch: 0
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.PLAY_SOUND_2D,
      expect.objectContaining({
        volume: 4,
        pitch: 0.01
      })
    );
  });

  it('clamps ambient playback volume and pitch to UE-safe ranges', async () => {
    await handleAudioTools(
      'create_ambient_sound',
      {
        action: 'create_ambient_sound',
        soundPath: '/Game/MCPTest/TestCue',
        volume: Number.NaN,
        pitch: Number.POSITIVE_INFINITY
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.CREATE_AMBIENT_SOUND,
      expect.objectContaining({
        volume: 1,
        pitch: 1
      })
    );
  });
});
