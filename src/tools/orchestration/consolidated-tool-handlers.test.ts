import { describe, expect, it } from 'vitest';
import { handleConsolidatedToolCall } from './consolidated-tool-handlers.js';
import { toolRegistry } from './dynamic-handler-registry.js';
import { createConnectedTools } from './consolidated-tool-handlers.test-support.js';

describe('consolidated action params compatibility', () => {
  it('merges params into level-structure payloads before validation and dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      params: {
        levelName: 'MCP_Racing_Level',
        levelPath: '/Game/MCP_Racing_Level',
        save: true
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      action: 'create_level',
      subAction: 'create_level',
      levelName: 'MCP_Racing_Level',
      levelPath: '/Game/MCP_Racing_Level',
      save: true
    }), expect.any(Object));
    const firstCall = sendAutomationRequest.mock.calls[0];
    expect(firstCall).toBeDefined();
    const payload = firstCall?.[1] ?? {};
    expect(payload).not.toHaveProperty('params');
  });

  it('lets top-level arguments override params when both are provided', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      levelName: 'TopLevelName',
      params: {
        action: 'create_sublevel',
        levelName: 'NestedName'
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      action: 'create_level',
      subAction: 'create_level',
      levelName: 'TopLevelName'
    }), expect.any(Object));
  });

  it('removes params before routing to strict input handlers', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_networking', {
      action: 'create_input_action',
      params: {
        name: 'IA_Throttle',
        path: '/Game/Input'
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_input', expect.objectContaining({
      action: 'create_input_action',
      subAction: 'create_input_action',
      name: 'IA_Throttle',
      path: '/Game/Input'
    }), expect.any(Object));
    const firstCall = sendAutomationRequest.mock.calls[0];
    expect(firstCall).toBeDefined();
    const payload = firstCall?.[1] ?? {};
    expect(payload).not.toHaveProperty('params');
  });

  it('routes base audio asset creation through audio authoring', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_audio', {
      action: 'create_sound_mix',
      name: 'SCM_Test',
      path: '/Game/Audio'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'create_sound_mix',
      name: 'SCM_Test',
      path: '/Game/Audio'
    }), expect.any(Object));
  });

  it('preserves sound cue aliases when routing through audio authoring', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_audio', {
      action: 'create_sound_cue',
      name: 'SC_TestCue',
      soundPath: '/Engine/VREditor/Sounds/VR_click1',
      savePath: '/Game/Audio/Cues'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'create_sound_cue',
      name: 'SC_TestCue',
      path: '/Game/Audio/Cues',
      wavePath: '/Engine/VREditor/Sounds/VR_click1'
    }), expect.any(Object));
  });

  it('forwards overwrite for level copy-style actions', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level', {
      action: 'duplicate_level',
      sourcePath: '/Game/Maps/Source',
      destinationPath: '/Game/Maps/Destination',
      overwrite: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level', expect.objectContaining({
      action: 'duplicate',
      sourcePath: '/Game/Maps/Source',
      destinationPath: '/Game/Maps/Destination',
      overwrite: true
    }), expect.any(Object));
  });

  it.each([
    ['configure_ray_traced_shadows', { enabled: true }],
    ['create_sphere_reflection_capture', { actorName: 'RT_Sphere' }],
    ['configure_pp_blend', { actorName: 'PP_Global', blendWeight: 0.75 }],
    ['create_scene_capture_2d', { actorName: 'Capture2D' }]
  ])('routes rendering build_environment action %s through manage_render', async (action, extraArgs) => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('build_environment', {
      action,
      ...extraArgs
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_render', expect.objectContaining({
      action,
      subAction: action
    }), expect.any(Object));
  });

  it('returns structured error context for unknown consolidated tools', async () => {
    const { tools } = createConnectedTools();

    const result = await handleConsolidatedToolCall('missing_tool', { action: 'probe' }, tools) as Record<string, unknown>;

    expect(result).toMatchObject({
      success: false,
      isError: true,
      error: 'UNKNOWN_TOOL',
      toolName: 'missing_tool',
      action: 'probe'
    });
    expect(String(result.message)).toContain('Unknown consolidated tool: missing_tool');
  });

  it('preserves tool and action context on dispatch exceptions', async () => {
    const { tools } = createConnectedTools();

    const result = await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      levelName: 'BadLevel',
      levelPath: '/etc/passwd'
    }, tools) as Record<string, unknown>;

    expect(result).toMatchObject({
      success: false,
      isError: true,
      error: 'SECURITY_VIOLATION',
      toolName: 'manage_level_structure',
      action: 'create_level'
    });
    expect(String(result.message)).toContain('Security violation');
  });

  it('preserves string messages from non-Error dispatch exceptions', async () => {
    const { tools } = createConnectedTools();
    toolRegistry.register('throw_object_test', async () => {
      throw { message: 'Security violation: object-shaped error' };
    });

    try {
      const result = await handleConsolidatedToolCall('throw_object_test', {
        action: 'probe'
      }, tools) as Record<string, unknown>;

      expect(result).toMatchObject({
        success: false,
        isError: true,
        error: 'SECURITY_VIOLATION',
        toolName: 'throw_object_test',
        action: 'probe'
      });
      expect(String(result.message)).toContain('Security violation: object-shaped error');
    } finally {
      toolRegistry.removeHandler('throw_object_test');
    }
  });
});
