import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../automation/index.js';
import type { ITools } from '../types/tool-interfaces.js';
import { handleConsolidatedToolCall } from './consolidated-tool-handlers.js';
import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';
import { coreToolDefinitions } from './schemas/core-tools.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<{ success: boolean }>;

function createConnectedTools() {
  const sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => ({ success: true }));
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources: {
      list: vi.fn(async () => ({}))
    },
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest
    } as unknown as AutomationBridge
  } as unknown as ITools;

  return { tools, sendAutomationRequest };
}

describe('consolidated action params compatibility', () => {
  it('advertises params for action tools in public schemas', () => {
    const tools = [
      consolidatedToolDefinitions.find((tool) => tool.name === 'manage_level_structure'),
      consolidatedToolDefinitions.find((tool) => tool.name === 'system_control'),
      coreToolDefinitions.find((tool) => tool.name === 'manage_level')
    ];

    for (const tool of tools) {
      const inputSchema = tool?.inputSchema as Record<string, unknown> | undefined;
      const properties = inputSchema?.properties as Record<string, unknown> | undefined;
      expect(properties).toHaveProperty('params');
      expect(inputSchema?.additionalProperties).toBe(true);
    }
  });

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

  it('routes full editor screenshot mode with base64 image return enabled', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true
    }, {});
  });

  it('forwards screenshot metadata opt-in for system control screenshots', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window',
      includeMetadata: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true,
      includeMetadata: true
    }, {});
  });
});
