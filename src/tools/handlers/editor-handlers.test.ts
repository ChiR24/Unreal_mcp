import { beforeEach, describe, expect, it, vi } from 'vitest';

import type { ITools } from '../../types/tool-interfaces.js';

vi.mock('./common-handlers.js', async (importOriginal) => {
  const actual = await importOriginal<typeof import('./common-handlers.js')>();

  return {
    ...actual,
    executeAutomationRequest: vi.fn()
  };
});

import { executeAutomationRequest } from './common-handlers.js';
import { handleEditorTools } from './editor-handlers.js';

const DESIGNER_MARQUEE_ASSET_PATH = '/Game/AdvancedIntegrationTest/WBP_DesignerMarquee';
const DESIGNER_MARQUEE_WINDOW_TITLE = 'WBP_DesignerMarquee';

describe('editor handlers', () => {
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

  it('forwards assetPath for simulate_input requests', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      type: 'mouse_drag'
    });

    await handleEditorTools(
      'simulate_input',
      {
        inputAction: 'mouse_drag',
        assetPath: DESIGNER_MARQUEE_ASSET_PATH,
        tabId: 'SlatePreview',
        windowTitle: DESIGNER_MARQUEE_WINDOW_TITLE,
        start: { clientX: 474, clientY: 163 },
        end: { clientX: 746, clientY: 255 }
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'simulate_input',
        type: 'mouse_drag',
        assetPath: DESIGNER_MARQUEE_ASSET_PATH,
        tabId: 'SlatePreview',
        windowTitle: DESIGNER_MARQUEE_WINDOW_TITLE,
        start: { clientX: 474, clientY: 163 },
        end: { clientX: 746, clientY: 255 }
      })
    );
  });
});