import { describe, expect, it } from 'vitest';

import type { ConsentGrant } from '../../../catalog/capabilities/semantic/authorization.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from './automation-request-dispatch.js';

const grant = { capability: 'manage_asset.delete_asset', acknowledge: 'explicit' } as unknown as ConsentGrant;

function toolsCapturing(capture: (options: unknown) => void): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (_action: string, _payload: Record<string, unknown>, options?: unknown) => {
        capture(options);
        return { success: true };
      }
    }
  } as unknown as ITools;
}

describe('executeAutomationRequest consent envelope sibling', () => {
  it('forwards the gateway consent control as an options sibling', async () => {
    let captured: { consent?: unknown } = {};
    const tools = toolsCapturing((options) => { captured = options as { consent?: unknown }; });

    await executeAutomationRequest(tools, 'manage_asset', { action: 'delete_asset' }, { consent: grant });

    expect(captured.consent).toEqual({ capability: 'manage_asset.delete_asset', acknowledge: 'explicit' });
  });

  it('omits consent when no consent control is given', async () => {
    let captured: { consent?: unknown } = {};
    const tools = toolsCapturing((options) => { captured = options as { consent?: unknown }; });

    await executeAutomationRequest(tools, 'manage_asset', { action: 'delete_asset' });

    expect(captured.consent).toBeUndefined();
  });
});
