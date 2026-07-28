import { describe, expect, it } from 'vitest';

import { runWithGatewayExpectedRevisions } from '../../../../automation/gateway-expected-revisions-context.js';
import { ExpectedRevisionsSchema } from '../../../catalog/capabilities/semantic/execution-options.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from './automation-request-dispatch.js';

function toolsCapturing(capture: (options: unknown) => void): ITools {
  return {
    systemTools: {
      executeConsoleCommand: async () => ({ success: true }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) },
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
  it('forwards active gateway pins without adding them to action params', async () => {
    const captured: unknown[] = [];
    const pins = ExpectedRevisionsSchema.parse({ selection: 7, package: 11 });

    await runWithGatewayExpectedRevisions(pins, () =>
      executeAutomationRequest(
        toolsCapturing((options) => captured.push(options)),
        'manage_asset',
        { action: 'rename_asset' }
      ));

    expect(captured).toEqual([
      expect.objectContaining({ expectedRevisions: { selection: 7, package: 11 } })
    ]);
  });

  it('omits expectedRevisions when no gateway pin context is active', async () => {
    const captured: unknown[] = [];

    await executeAutomationRequest(
      toolsCapturing((options) => captured.push(options)),
      'inspect',
      { action: 'get_object_details' }
    );

    expect(captured).toEqual([expect.not.objectContaining({ expectedRevisions: expect.anything() })]);
  });
});
