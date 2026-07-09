import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/foundation/dispatch/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn()
}));

import { handleBlueprintGet } from '../../../src/tools/handlers/blueprint/blueprint-handlers.js';
import { blueprintEventHandlers } from '../../../src/tools/handlers/blueprint/blueprint-event-actions.js';
import type { BlueprintActionContext } from '../../../src/tools/handlers/blueprint/blueprint-action-context.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/foundation/dispatch/common-handlers.js';

describe('Blueprint Handlers', () => {
  const mockExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = {
      systemTools: {
        executeConsoleCommand: vi.fn(async () => ({ success: true })),
        getProjectSettings: vi.fn(async () => ({}))
      },
      assetResources: {
        list: vi.fn(async () => ({}))
      },
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn()
      }
    };
  });

  it('preserves rich blueprint_get variable details in wrapped response', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      message: 'Blueprint fetched',
      resolvedPath: '/Game/Abilities/Shared/AS_CharacterStats',
      defaults: {
        Strength_Min: '0.0',
        StatsComponent: 'None'
      },
      variables: [
        {
          name: 'Strength_Min',
          type: 'float',
          inherited: false,
          metadata: { tooltip: 'Minimum strength' }
        },
        {
          name: 'StatsComponent',
          type: 'UActorComponent*',
          component: true,
          inherited: true,
          declaredInBlueprintPath: '/Game/Abilities/Base/BP_BaseStats'
        }
      ]
    });

    const result = await handleBlueprintGet(
      { blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'blueprint_get',
      { blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats' },
      'Automation bridge not available for blueprint operations'
    );
    expect(result).toEqual({
      success: true,
      message: 'Blueprint fetched',
      blueprintPath: '/Game/Abilities/Shared/AS_CharacterStats',
      blueprint: {
        resolvedPath: '/Game/Abilities/Shared/AS_CharacterStats',
        defaults: {
          Strength_Min: '0.0',
          StatsComponent: 'None'
        },
        variables: [
          {
            name: 'Strength_Min',
            type: 'float',
            inherited: false,
            metadata: { tooltip: 'Minimum strength' }
          },
          {
            name: 'StatsComponent',
            type: 'UActorComponent*',
            component: true,
            inherited: true,
            declaredInBlueprintPath: '/Game/Abilities/Base/BP_BaseStats'
          }
        ]
      }
    });
  });

  it('remove_function with { blueprintPath, name } targets the blueprint, not the function name', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, message: 'Function removed' });

    const args = { blueprintPath: '/Game/Test/BP_Thing', name: 'MyFunction' };
    const context = { argsTyped: args, argsRecord: args, tools: mockTools } as unknown as BlueprintActionContext;

    await blueprintEventHandlers.remove_function(context);

    // Regression: blueprintTarget() resolves name-first, so without disambiguation the
    // function name "MyFunction" would be sent as the Blueprint path. The path must
    // resolve to blueprintPath, and `name` must be used as the function name instead.
    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'blueprint_remove_function',
      expect.objectContaining({
        requestedPath: '/Game/Test/BP_Thing',
        blueprintCandidates: ['/Game/Test/BP_Thing'],
        functionName: 'MyFunction'
      }),
      undefined
    );
  });
});
