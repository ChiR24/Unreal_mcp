import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleActorTools } from './actor-handlers.js';

function createConnectedTools(result: Record<string, unknown>) {
  const sendAutomationRequest = vi.fn(async () => result);
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
  };

  return { tools, sendAutomationRequest };
}

describe('handleActorTools list', () => {
  it('forwards list filters to Unreal', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({
      success: true,
      result: { actors: [], count: 0, totalCount: 0 }
    });

    await handleActorTools('list', { action: 'list', limit: 10, filter: 'Light' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_actor', {
      action: 'list',
      limit: 10,
      filter: 'Light'
    }, {});
  });

  it('normalizes invalid list limits before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({
      success: true,
      result: { actors: [], count: 0, totalCount: 0 }
    });

    await handleActorTools('list', { action: 'list', limit: Number.NaN }, tools);
    await handleActorTools('list', { action: 'list', limit: -5 }, tools);
    await handleActorTools('list', { action: 'list', limit: 10.9 }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'control_actor', expect.objectContaining({ limit: 50 }), {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'control_actor', expect.objectContaining({ limit: 50 }), {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(3, 'control_actor', expect.objectContaining({ limit: 10 }), {});
  });

  it('exposes actor-list result fields in the public schemas', async () => {
    const { consolidatedToolDefinitions } = await import('../consolidated-tool-definitions.js');
    const { coreToolDefinitions } = await import('../schemas/core-tools.js');
    const tools = [
      consolidatedToolDefinitions.find((tool) => tool.name === 'control_actor'),
      coreToolDefinitions.find((tool) => tool.name === 'control_actor')
    ];

    for (const tool of tools) {
      const inputProperties = (tool?.inputSchema as Record<string, unknown> & {
        properties: Record<string, unknown>;
      }).properties;
      const outputProperties = (tool?.outputSchema as Record<string, unknown> & {
        properties: Record<string, unknown>;
      }).properties;

      expect(inputProperties).toHaveProperty('filter');
      expect(inputProperties).toHaveProperty('limit');
      expect(outputProperties).toHaveProperty('actors');
      expect(outputProperties).toHaveProperty('count');
      expect(outputProperties).toHaveProperty('totalCount');
      expect(outputProperties).toHaveProperty('isPieWorld');
      expect(outputProperties).toHaveProperty('worldName');
      expect(outputProperties).toHaveProperty('filter');
    }
  });

  it('promotes direct actor list data from the Unreal response result', async () => {
    const { tools } = createConnectedTools({
      success: true,
      result: {
        actors: [{ label: 'DefaultPawn_0', name: 'DefaultPawn_0' }],
        count: 1,
        totalCount: 84,
        isPieWorld: true,
        worldName: 'UEDPIE_0_Untitled_1'
      }
    });

    const result = await handleActorTools('list', { action: 'list', limit: 1 }, tools);

    expect(result.actors).toEqual([{ label: 'DefaultPawn_0', name: 'DefaultPawn_0' }]);
    expect(result.count).toBe(1);
    expect(result.totalCount).toBe(84);
    expect(result.isPieWorld).toBe(true);
    expect(result.worldName).toBe('UEDPIE_0_Untitled_1');
    expect(result.message).toBe('Found 84 actors: DefaultPawn_0... and 83 more');
  });

  it('does not rewrite failed list responses even when result data includes actors', async () => {
    const { tools } = createConnectedTools({
      success: false,
      message: 'Handler reported success but Unreal logged errors',
      error: 'ENGINE_ERROR',
      result: {
        actors: [{ label: 'DefaultPawn_0', name: 'DefaultPawn_0' }],
        count: 1,
        totalCount: 84,
        success: false,
        engineErrors: ['[LogEditor] failure']
      }
    });

    const result = await handleActorTools('list', { action: 'list', limit: 1 }, tools);

    expect(result.success).toBe(false);
    expect(result.message).toBe('Handler reported success but Unreal logged errors');
    expect(result.error).toBe('ENGINE_ERROR');
    expect(result.actors).toBeUndefined();
  });

  it('promotes actor list data from the standard Unreal response envelope', async () => {
    const { tools } = createConnectedTools({
      success: true,
      result: {
        success: true,
        data: {
          actors: [{ label: 'DefaultPawn_0', name: 'DefaultPawn_0' }],
          count: 1,
          totalCount: 84,
          isPieWorld: true
        },
        warnings: [],
        error: null
      }
    });

    const result = await handleActorTools('list', { action: 'list', limit: 1 }, tools);

    expect(result.actors).toEqual([{ label: 'DefaultPawn_0', name: 'DefaultPawn_0' }]);
    expect(result.count).toBe(1);
    expect(result.totalCount).toBe(84);
    expect(result.message).toBe('Found 84 actors: DefaultPawn_0... and 83 more');
  });
});

describe('handleActorTools apply_force', () => {
  it('auto-enables physics on a mesh component and retries apply_force', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true });
    sendAutomationRequest
      .mockRejectedValueOnce(new Error('PHYSICS simulation is disabled'))
      .mockResolvedValueOnce({
        success: true,
        components: [{ name: 'StaticMeshComponent0' }]
      })
      .mockResolvedValueOnce({ success: true })
      .mockResolvedValueOnce({ success: true, impulseApplied: true });

    const result = await handleActorTools('apply_force', {
      action: 'apply_force',
      actorName: 'MCP_PhysicsActor',
      force: { x: 0, y: 0, z: 2500 }
    }, tools);

    expect(result).toEqual({ success: true, impulseApplied: true });
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'control_actor', {
      action: 'apply_force',
      actorName: 'MCP_PhysicsActor',
      force: { x: 0, y: 0, z: 2500 }
    }, {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'control_actor', {
      action: 'get_components',
      actorName: 'MCP_PhysicsActor'
    }, {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(3, 'control_actor', {
      action: 'set_component_properties',
      actorName: 'MCP_PhysicsActor',
      componentName: 'StaticMeshComponent0',
      properties: { SimulatePhysics: true, bSimulatePhysics: true, Mobility: 2 }
    }, {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(4, 'control_actor', {
      action: 'apply_force',
      actorName: 'MCP_PhysicsActor',
      force: { x: 0, y: 0, z: 2500 }
    }, {});
  });
});
