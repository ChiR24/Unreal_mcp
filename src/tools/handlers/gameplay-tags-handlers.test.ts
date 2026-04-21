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

describe('manage_gameplay_tags add_gameplay_tag', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      tag: 'Modifier.Weather.Rain',
      sourceIni: 'DefaultGameplayTags.ini',
    });
  });

  it('forwards tag + comment + default sourceIni to the automation bridge', async () => {
    const res = await handleGameplayTagsTools(
      'add_gameplay_tag',
      { tag: 'Modifier.Weather.Rain', comment: 'Rain modifier' } as unknown as Record<string, unknown>,
      {} as never
    );

    expect(res.success).toBe(true);
    expect(res.tag).toBe('Modifier.Weather.Rain');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    expect(calls[0][1]).toBe('manage_gameplay_tags');
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('add_gameplay_tag');
    expect(payload.tag).toBe('Modifier.Weather.Rain');
    expect(payload.comment).toBe('Rain modifier');
    expect(payload.sourceIni).toBe('DefaultGameplayTags.ini');
  });

  it('forwards explicit sourceIni when provided', async () => {
    await handleGameplayTagsTools(
      'add_gameplay_tag',
      { tag: 'Combat.Ability.Slash', sourceIni: 'CombatTags.ini' } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.sourceIni).toBe('CombatTags.ini');
    expect(payload.comment).toBe('');
  });

  it('throws on missing tag', async () => {
    await expect(
      handleGameplayTagsTools(
        'add_gameplay_tag',
        {} as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/tag/);
  });
});

describe('manage_gameplay_tags list_gameplay_tags', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      tags: ['Modifier.Weather.Rain', 'Modifier.Weather.Snow'],
    });
  });

  it('forwards empty payload when no prefix supplied', async () => {
    const res = await handleGameplayTagsTools(
      'list_gameplay_tags',
      {} as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.success).toBe(true);
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('list_gameplay_tags');
    expect(payload.prefix).toBeUndefined();
  });

  it('forwards prefix filter to the automation bridge', async () => {
    await handleGameplayTagsTools(
      'list_gameplay_tags',
      { prefix: 'Modifier.Weather' } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.prefix).toBe('Modifier.Weather');
  });
});

describe('manage_gameplay_tags remove_gameplay_tag', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('forwards tag to the automation bridge', async () => {
    await handleGameplayTagsTools(
      'remove_gameplay_tag',
      { tag: 'Modifier.Weather.Rain' } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('remove_gameplay_tag');
    expect(payload.tag).toBe('Modifier.Weather.Rain');
  });

  it('throws on missing tag', async () => {
    await expect(
      handleGameplayTagsTools(
        'remove_gameplay_tag',
        {} as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/tag/);
  });
});
