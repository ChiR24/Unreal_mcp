import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn()
}));

import { handleBlueprintGet } from '../../../src/tools/handlers/blueprint-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('Blueprint Handlers', () => {
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
});
