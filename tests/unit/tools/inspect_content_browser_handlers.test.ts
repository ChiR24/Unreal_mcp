import {
  beforeEach,
  describe,
  expect,
  it,
  vi,
} from 'vitest';

import { executeAutomationRequest } from '../../../src/tools/handlers/foundation/dispatch/common-handlers.js';
import { handleInspectTools } from '../../../src/tools/handlers/inspect/inspect-handlers.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/foundation/dispatch/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

describe('Inspect Content Browser Handlers', () => {
  const mockExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = {
      systemTools: {
        executeConsoleCommand: vi.fn(async () => ({ success: true })),
        getProjectSettings: vi.fn(async () => ({})),
      },
      assetResources: {
        list: vi.fn(async () => ({})),
      },
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn(),
      },
    };
  });

  it('routes get_content_browser_state through inspect automation', async () => {
    const contentBrowserState = {
      success: true,
      currentPath: '/Game/Characters',
      selectedFolders: ['/Game/Characters/Heroes'],
      selectedAssets: [{
        assetName: 'BP_Hero',
        objectPath: '/Game/Characters/Heroes/BP_Hero.BP_Hero',
        packagePath: '/Game/Characters/Heroes',
        classPath: '/Script/Engine.Blueprint',
      }],
    };
    mockExecuteAutomationRequest.mockResolvedValue(contentBrowserState);

    const result = await handleInspectTools(
      'get_content_browser_state',
      {},
      mockTools,
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'inspect',
      { action: 'get_content_browser_state' },
    );
    expect(result).toEqual(contentBrowserState);
  });

  it('publishes get_content_browser_state in both inspect schemas', async () => {
    const { consolidatedToolDefinitions } = await import(
      '../../../src/tools/catalog/consolidated-tool-definitions.js'
    );
    const { coreToolDefinitions } = await import(
      '../../../src/tools/schemas/core-tools.js'
    );
    const inspectTools = [
      consolidatedToolDefinitions.find(({ name }) => name === 'inspect'),
      coreToolDefinitions.find(({ name }) => name === 'inspect'),
    ];

    for (const inspectTool of inspectTools) {
      const actionSchema = inspectTool?.inputSchema.properties.action;
      expect(actionSchema).toHaveProperty(
        'enum',
        expect.arrayContaining(['get_content_browser_state']),
      );
    }
  });

  it('publishes Content Browser output fields in both inspect schemas', async () => {
    const { consolidatedToolDefinitions } = await import(
      '../../../src/tools/catalog/consolidated-tool-definitions.js'
    );
    const { coreToolDefinitions } = await import(
      '../../../src/tools/schemas/core-tools.js'
    );
    const inspectTools = [
      consolidatedToolDefinitions.find(({ name }) => name === 'inspect'),
      coreToolDefinitions.find(({ name }) => name === 'inspect'),
    ];
    const expectedFields = [
      'currentPath',
      'selectedFolders',
      'selectedFolderCount',
      'selectedAssets',
      'selectedAssetCount',
    ];

    for (const inspectTool of inspectTools) {
      expect(inspectTool?.outputSchema.properties).toEqual(
        expect.objectContaining(
          Object.fromEntries(expectedFields.map((field) => [field, expect.any(Object)])),
        ),
      );
    }
  });
});
