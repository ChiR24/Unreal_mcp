import { afterEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { clearEditorContentRoots, setEditorContentRoots } from '../../../../utils/paths/path-security.js';
import { executeAutomationRequest } from './automation-request-dispatch.js';

function createConnectedTools() {
  const sendAutomationRequest = vi.fn(async () => ({ success: true }));
  const tools: ITools = { automationBridge: { isConnected: () => true, sendAutomationRequest } };
  return { tools, sendAutomationRequest };
}

describe('executeAutomationRequest console command validation', () => {
  it('blocks an unsafe console_command action before sending to the bridge', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'system_control', { action: 'console_command', command: 'py print("unsafe")' }))
      .rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('blocks an unsafe command sent to the bare console_command tool', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'console_command', { command: 'quit' }))
      .rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('sends safe console commands after validation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(tools, 'system_control', { action: 'console_command', command: 'stat fps' });

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'system_control',
      { action: 'console_command', command: 'stat fps' },
      { timeoutMs: expect.any(Number) }
    );
  });

  it.each([
    ['set_cvar', { name: 'r.VSync', value: 'import os' }],
    ['set_cvar', { command: 'py print(1)' }],
    ['set_resolution', { resolution: '1920x1080 & quit' }],
    ['set_fullscreen', { resolution: '1920x1080; quit' }],
  ])('blocks an unsafe console command %s would compose', async (action, params) => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'system_control', { action, ...params }))
      .rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it.each([
    ['set_cvar', { name: 'r.VSync', value: 1 }],
    ['set_cvar', { cvar: 't.MaxFPS', value: true }],
    ['set_resolution', { resolution: '1920x1080' }],
    ['set_fullscreen', { width: 1920, height: 1080 }],
  ])('sends a safe %s after validating its composed command', async (action, params) => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(tools, 'system_control', { action, ...params });

    expect(sendAutomationRequest).toHaveBeenCalledTimes(1);
  });

  it('keeps start_render above an operator timeout pin', async () => {
    vi.stubEnv('MCP_REQUEST_TIMEOUT_MS', '30000');
    try {
      const { tools, sendAutomationRequest } = createConnectedTools();

      await executeAutomationRequest(tools, 'manage_sequence', { action: 'start_render' });
      await executeAutomationRequest(tools, 'manage_sequence', { action: 'play_sequence' });

      expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_sequence', { action: 'start_render' }, { timeoutMs: 335000 });
      expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_sequence', { action: 'play_sequence' }, { timeoutMs: 30000 });
    } finally {
      vi.unstubAllEnvs();
    }
  });

  it("uses the caller's timeoutMs control over the cost tier", async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(tools, 'manage_sequence', { action: 'start_render' }, { timeoutMs: 65000 });

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_sequence', { action: 'start_render' }, { timeoutMs: 65000 });
  });
});

describe('executeAutomationRequest path gate with editor-reported content roots', () => {
  afterEach(() => {
    clearEditorContentRoots();
  });

  // The gateway sends { ...params, action } (no subAction), so these are the
  // payloads a gateway execute really produces.
  it('sends a content path under a reported mount, and refuses it without the mount', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();
    const args = { path: '/ShooterCore/X', action: 'list' };

    await expect(executeAutomationRequest(tools, 'manage_asset', args)).rejects.toThrow(/unauthorized absolute path/);
    expect(sendAutomationRequest).not.toHaveBeenCalled();

    setEditorContentRoots(['/Game', '/ShooterCore']);
    await executeAutomationRequest(tools, 'manage_asset', args);
    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_asset', args, { timeoutMs: expect.any(Number) });
  });

  it('refuses a reported mount as the file an import reads', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();
    setEditorContentRoots(['/Game', '/ShooterCore']);

    await expect(executeAutomationRequest(tools, 'manage_asset', {
      sourcePath: '/ShooterCore/x.fbx',
      destinationPath: '/Game/X',
      action: 'import',
    })).rejects.toThrow(/unauthorized absolute path/);
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });
});
