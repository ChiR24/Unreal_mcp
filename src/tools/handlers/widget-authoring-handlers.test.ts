import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async (): Promise<Record<string, unknown>> => ({
    success: true,
    widgetInfo: { tree: { name: 'Root', class: 'UCanvasPanel', children: [] } }
  }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';

describe('widget_authoring get_widget_info tree response', () => {
  beforeEach(() => executeAutomationRequestMock.mockClear());

  it('passes through tree field in widgetInfo', async () => {
    const result = await handleWidgetAuthoringTools(
      'get_widget_info',
      { action: 'get_widget_info', widgetPath: '/Game/UI/WBP_Foo' },
      {} as never
    ) as { widgetInfo?: { tree?: unknown } };

    expect(result.widgetInfo?.tree).toMatchObject({
      name: expect.any(String),
      class: expect.any(String),
      children: expect.any(Array)
    });
  });
});
