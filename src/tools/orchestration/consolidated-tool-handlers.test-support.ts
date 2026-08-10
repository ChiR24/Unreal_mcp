// src/tools/orchestration/consolidated-tool-handlers.test-support.ts
// Shared connected-bridge fixture for the consolidated tool handler suites.
// Not collected by Vitest: the include glob is `src/**/*.test.ts`.

import { vi } from 'vitest';
import type { ITools } from '../../types/tools/tool-interfaces.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<{ success: boolean }>;

export function createConnectedTools() {
  const sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => ({ success: true }));
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
    }
  };

  return { tools, sendAutomationRequest };
}
