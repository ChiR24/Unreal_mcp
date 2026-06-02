import { afterEach, describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { createSubActionDispatcher, executeAutomationRequest, getTimeoutMs, normalizePathFields, validateSecurityPatterns } from './common-handlers.js';

vi.mock('../../config.js', async (importOriginal) => {
  const actual = await importOriginal<typeof import('../../config.js')>();
  return {
    ...actual,
    getAdditionalPathPrefixes: () => ['/ProjectObject/']
  };
});

function createConnectedTools() {
  const sendAutomationRequest = vi.fn(async () => ({ success: true }));
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

describe('getTimeoutMs', () => {
  const originalCanonicalTimeout = process.env.MCP_REQUEST_TIMEOUT_MS;
  const originalTimeout = process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;

  afterEach(() => {
    if (originalCanonicalTimeout === undefined) {
      delete process.env.MCP_REQUEST_TIMEOUT_MS;
    } else {
      process.env.MCP_REQUEST_TIMEOUT_MS = originalCanonicalTimeout;
    }
    if (originalTimeout === undefined) {
      delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
    } else {
      process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = originalTimeout;
    }
  });

  it('uses the default when the timeout env var is not set', () => {
    delete process.env.MCP_REQUEST_TIMEOUT_MS;
    delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;

    expect(getTimeoutMs(1234)).toBe(1234);
  });

  it('accepts canonical positive decimal integer strings', () => {
    process.env.MCP_REQUEST_TIMEOUT_MS = '60000';

    expect(getTimeoutMs(1234)).toBe(60000);
  });

  it('accepts the legacy automation-specific timeout alias', () => {
    delete process.env.MCP_REQUEST_TIMEOUT_MS;
    process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = '45000';

    expect(getTimeoutMs(1234)).toBe(45000);
  });

  it('prefers the canonical timeout over the legacy alias', () => {
    process.env.MCP_REQUEST_TIMEOUT_MS = '60000';
    process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = '45000';

    expect(getTimeoutMs(1234)).toBe(60000);
  });

  it('rejects partial, non-decimal, fractional, zero, and negative strings', () => {
    for (const value of ['5000ms', '0x1388', '5e3', '100.5', '0', '-1']) {
      process.env.MCP_REQUEST_TIMEOUT_MS = value;

      expect(getTimeoutMs(1234)).toBe(1234);
    }
  });
});

describe('normalizePathFields', () => {
  it('maps Content and Game root aliases without duplicating /Game', () => {
    const normalized = normalizePathFields({
      contentPath: 'Content/Foo/Bar',
      gamePath: 'Game/Foo/Bar',
      enginePath: 'Engine/EngineMaterials/DefaultMaterial',
      niagaraPath: 'Niagara/Modules/EmitterState',
      pluginPath: 'ProjectObject/Materials/M_Test',
      windowsContentPath: 'Content\\Foo\\Bar',
      barePath: 'Foo/Bar'
    }, ['contentPath', 'gamePath', 'enginePath', 'niagaraPath', 'pluginPath', 'windowsContentPath', 'barePath']);

    expect(normalized.contentPath).toBe('/Game/Foo/Bar');
    expect(normalized.gamePath).toBe('/Game/Foo/Bar');
    expect(normalized.enginePath).toBe('/Engine/EngineMaterials/DefaultMaterial');
    expect(normalized.niagaraPath).toBe('/Niagara/Modules/EmitterState');
    expect(normalized.pluginPath).toBe('/ProjectObject/Materials/M_Test');
    expect(normalized.windowsContentPath).toBe('/Game/Foo/Bar');
    expect(normalized.barePath).toBe('/Game/Foo/Bar');
  });

  it('blocks parent-directory path segments after alias normalization', () => {
    for (const value of ['..', 'Foo/..', 'Foo\\..', '/Game/..', '/Game/Foo/../Bar']) {
      const normalized = normalizePathFields({ assetPath: value }, ['assetPath']);

      expect(validateSecurityPatterns(normalized)).toContain('Path traversal is not allowed');
    }
  });

  it('allows dot characters inside normal path segments', () => {
    expect(validateSecurityPatterns({ assetPath: '/Game/Foo..Bar/Baz' })).toBeUndefined();
  });
});

describe('executeAutomationRequest console command validation', () => {
  it('blocks unsafe console_command payloads before sending to the bridge', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'console_command', { command: 'py print("unsafe")' }))
      .rejects.toThrow(/Python console commands are blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('validates each batch_console_commands entry before sending', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'batch_console_commands', {
      commands: ['stat fps', { command: 'quit' }]
    })).rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('validates cmd when a batch command object has an empty command alias', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    for (const command of ['', '   ']) {
      await expect(executeAutomationRequest(tools, 'batch_console_commands', {
        commands: [{ command, cmd: 'quit' }]
      })).rejects.toThrow(/Dangerous command blocked/);
    }

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('sends safe console commands after validation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(tools, 'console_command', { command: 'stat fps' });

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', { command: 'stat fps' }, {});
  });
});

describe('createSubActionDispatcher', () => {
  it('normalizes configured paths, applies payload preparation, and uses dispatcher timeout', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();
    const dispatcher = createSubActionDispatcher(tools, {
      action: 'outer_action',
      assetPath: 'Content\\UI\\WBP_Menu',
      timeoutMs: 50
    }, {
      toolName: 'manage_widget_authoring',
      domainName: 'widget authoring',
      pathFields: ['assetPath', 'folderPath'],
      timeoutMs: 900,
      preparePayload: (payload, subAction) => ({
        ...payload,
        preparedSubAction: subAction
      })
    });

    await dispatcher.sendRequest('create_widget', {
      folderPath: 'ProjectObject\\Menus'
    });

    expect(dispatcher.argsRecord.assetPath).toBe('/Game/UI/WBP_Menu');
    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_widget_authoring', {
      action: 'outer_action',
      assetPath: '/Game/UI/WBP_Menu',
      folderPath: '/ProjectObject/Menus',
      subAction: 'create_widget',
      preparedSubAction: 'create_widget'
    }, { timeoutMs: 900 });
  });
});
