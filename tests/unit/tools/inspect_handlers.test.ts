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
