import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(
    async (): Promise<Record<string, unknown>> => ({ success: true })
  ),
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  },
}));

import { handleGameplayTagsTools } from './gameplay-tags-handlers.js';

describe('manage_gameplay_tags skeleton', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('throws Unsupported for unrecognised actions', async () => {
    await expect(
      handleGameplayTagsTools('__not_a_real_action__', {} as never, {} as never)
    ).rejects.toThrow(/Unsupported manage_gameplay_tags action/);
  });
});
