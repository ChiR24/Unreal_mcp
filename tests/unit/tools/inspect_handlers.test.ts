import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn()
}));

import { handleInspectTools } from '../../../src/tools/handlers/inspect-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('Inspect Handlers', () => {
  const mockExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn()
      }
    } as unknown as ITools;
  });

  it('routes get_blueprint_details through blueprint_get', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, variables: [{ name: 'Strength_Min' }] });

    const result = await handleInspectTools(
      'get_blueprint_details',
      { objectPath: '/Game/Abilities/Shared/AS_CharacterStats' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'blueprint_get',
      {
        requestedPath: '/Game/Abilities/Shared/AS_CharacterStats',
        blueprintCandidates: ['/Game/Abilities/Shared/AS_CharacterStats']
      },
      'inspect:get_blueprint_details -> blueprint_get: automation bridge not available'
    );
    expect(result).toEqual({ success: true, variables: [{ name: 'Strength_Min' }] });
  });

  describe('inspect_cdo', () => {
    it('routes inspect_cdo with blueprintPath to inspect automation', async () => {
      mockExecuteAutomationRequest.mockResolvedValue({
        success: true,
        className: 'BP_Hero_C',
        components: [{ name: 'CharacterMesh0', class: 'SkeletalMeshComponent' }]
      });

      const result = await handleInspectTools(
        'inspect_cdo',
        { blueprintPath: '/Game/Characters/BP_Hero' },
        mockTools
      );

      const payload = mockExecuteAutomationRequest.mock.calls[0][2] as Record<string, unknown>;
      expect(payload.action).toBe('inspect_cdo');
      expect(payload.blueprintPath).toBe('/Game/Characters/BP_Hero');
      expect(result).toEqual({
        success: true,
        className: 'BP_Hero_C',
        components: [{ name: 'CharacterMesh0', class: 'SkeletalMeshComponent' }]
      });
    });

    it('delegates to C++ when blueprintPath is missing', async () => {
      mockExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'blueprintPath is required for inspect_cdo'
      });

      const result = await handleInspectTools(
        'inspect_cdo',
        {},
        mockTools
      );

      expect(mockExecuteAutomationRequest).toHaveBeenCalled();
      expect(result).toEqual({
        success: false,
        error: 'blueprintPath is required for inspect_cdo'
      });
    });

    it('passes optional detailed and componentName params', async () => {
      mockExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleInspectTools(
        'inspect_cdo',
        { blueprintPath: '/Game/Characters/BP_Hero', detailed: true, componentName: 'CharacterMesh0' },
        mockTools
      );

      const payload = mockExecuteAutomationRequest.mock.calls[0][2] as Record<string, unknown>;
      expect(payload.action).toBe('inspect_cdo');
      expect(payload.blueprintPath).toBe('/Game/Characters/BP_Hero');
      expect(payload.detailed).toBe(true);
      expect(payload.componentName).toBe('CharacterMesh0');
    });

    it('passes propertyNames array when provided', async () => {
      mockExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleInspectTools(
        'inspect_cdo',
        { blueprintPath: '/Game/Characters/BP_Hero', propertyNames: ['SkeletalMesh', 'AnimClass'] },
        mockTools
      );

      const payload = mockExecuteAutomationRequest.mock.calls[0][2] as Record<string, unknown>;
      expect(payload.action).toBe('inspect_cdo');
      expect(payload.propertyNames).toEqual(['SkeletalMesh', 'AnimClass']);
    });

    it('falls back to objectPath when blueprintPath not provided', async () => {
      mockExecuteAutomationRequest.mockResolvedValue({ success: true });

      await handleInspectTools(
        'inspect_cdo',
        { objectPath: '/Game/Characters/BP_Hero' },
        mockTools
      );

      const payload = mockExecuteAutomationRequest.mock.calls[0][2] as Record<string, unknown>;
      expect(payload.action).toBe('inspect_cdo');
      expect(payload.blueprintPath).toBe('/Game/Characters/BP_Hero');
    });
  });

  it('inspect_cdo is in the tool schema action enum', async () => {
    const { consolidatedToolDefinitions } = await import('../../../src/tools/consolidated-tool-definitions.js');
    const inspectTool = consolidatedToolDefinitions.find((t: { name: string }) => t.name === 'inspect');
    const actionEnum = (inspectTool?.inputSchema as Record<string, unknown> & {
      properties: { action: { enum: string[] } }
    })?.properties?.action?.enum;
    expect(actionEnum).toContain('inspect_cdo');
  });

  it('keeps unsupported inspect export file-path params out of the schema', async () => {
    const { consolidatedToolDefinitions } = await import('../../../src/tools/consolidated-tool-definitions.js');
    const { coreToolDefinitions } = await import('../../../src/tools/schemas/core-tools.js');
    const inspectTool = consolidatedToolDefinitions.find((t: { name: string }) => t.name === 'inspect');
    const coreInspectTool = coreToolDefinitions.find((t: { name: string }) => t.name === 'inspect');
    const tools = [inspectTool, coreInspectTool];

    for (const tool of tools) {
      const properties = (tool?.inputSchema as Record<string, unknown> & {
        properties: Record<string, unknown>
      })?.properties;
      expect(properties).not.toHaveProperty('destinationPath');
      expect(properties).not.toHaveProperty('outputPath');
      expect(properties).not.toHaveProperty('format');
    }
  });

  it('normalizes name and propertyPath aliases for get_property', async () => {
    mockExecuteAutomationRequest
      .mockResolvedValueOnce({ success: true, actors: [{ path: '/Game/Test/ResolvedActor' }] })
      .mockResolvedValueOnce({ success: true, value: 0 });

    await handleInspectTools(
      'get_property',
      { name: 'ReadableActorName', propertyPath: 'InitialLifeSpan' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      2,
      mockTools,
      'inspect',
      {
        name: 'ReadableActorName',
        propertyPath: 'InitialLifeSpan',
        action: 'get_property',
        propertyName: 'InitialLifeSpan',
        objectPath: '/Game/Test/ResolvedActor'
      }
    );
  });

  it('normalizes classPath alias for inspect_class', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, classPath: '/Script/Engine.StaticMeshActor' });

    await handleInspectTools(
      'inspect_class',
      { classPath: '/Script/Engine.StaticMeshActor' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'inspect',
      {
        action: 'inspect_class',
        className: '/Script/Engine.StaticMeshActor'
      }
    );
  });

  it('keeps inspect_object on the inspect automation path', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, objectPath: '/Game/Test/BP_Test' });

    await handleInspectTools(
      'inspect_object',
      { objectPath: '/Game/Test/BP_Test' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'inspect',
      {
        objectPath: '/Game/Test/BP_Test',
        action: 'inspect_object',
        detailed: true
      },
      'Automation bridge not available for inspect operations'
    );
  });
});
