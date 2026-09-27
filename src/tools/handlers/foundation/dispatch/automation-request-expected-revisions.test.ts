import { describe, expect, it } from 'vitest';

import { ExpectedRevisionsSchema } from '../../../catalog/capabilities/semantic/execution-options.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from './automation-request-dispatch.js';

function toolsCapturing(capture: (options: unknown) => void): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (_action, _payload, options) => {
        capture(options);
        return { success: true };
      }
    }
  };
}

describe('executeAutomationRequest expected-revisions envelope sibling', () => {
  it('forwards gateway pins as an options sibling, never as action params', async () => {
    const captured: unknown[] = [];
    const payloads: unknown[] = [];
    const pins = ExpectedRevisionsSchema.parse({ selection: 7, package: 11 });
    const tools: ITools = {
      automationBridge: {
        isConnected: () => true,
        sendAutomationRequest: async (_action, payload, options) => {
          payloads.push(payload);
          captured.push(options);
          return { success: true };
        }
      }
    };

    await executeAutomationRequest(tools, 'manage_asset', { action: 'rename_asset' }, { expectedRevisions: pins });

    expect(captured).toEqual([expect.objectContaining({ expectedRevisions: { selection: 7, package: 11 } })]);
    expect(payloads).toEqual([{ action: 'rename_asset' }]);
  });

  it('omits expectedRevisions when no pins are given', async () => {
    const captured: unknown[] = [];

    await executeAutomationRequest(toolsCapturing((options) => captured.push(options)), 'inspect', { action: 'get_object_details' });

    expect(captured).toEqual([expect.not.objectContaining({ expectedRevisions: expect.anything() })]);
  });
});
