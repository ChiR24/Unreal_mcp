import { afterEach, describe, expect, it, vi } from 'vitest';
import type { Logger } from './logger.js';
import { createElicitationHelper, type ElicitSchema } from './elicitation.js';

describe('createElicitationHelper timeout env parsing', () => {
  afterEach(() => {
    vi.unstubAllEnvs();
  });

  it('rejects partial numeric timeout strings', () => {
    vi.stubEnv('MCP_ELICITATION_TIMEOUT_MS', '120000ms');
    vi.stubEnv('ELICITATION_TIMEOUT_MS', '');
    const helper = createElicitationHelper({}, { debug: vi.fn() } as unknown as Logger);

    expect(helper.getDefaultTimeoutMs()).toBe(180000);
  });

  it('accepts positive decimal integer timeout strings', () => {
    vi.stubEnv('MCP_ELICITATION_TIMEOUT_MS', '120000');
    vi.stubEnv('ELICITATION_TIMEOUT_MS', '');
    const helper = createElicitationHelper({}, { debug: vi.fn() } as unknown as Logger);

    expect(helper.getDefaultTimeoutMs()).toBe(120000);
  });

  it('caps per-request timeout overrides at the supported maximum', async () => {
    vi.stubEnv('MCP_ELICITATION_TIMEOUT_MS', '');
    vi.stubEnv('ELICITATION_TIMEOUT_MS', '');
    const elicitInput = vi.fn(async () => ({ action: 'accept', content: { ok: true } }));
    const helper = createElicitationHelper({ elicitInput }, { debug: vi.fn() } as unknown as Logger);
    const schema: ElicitSchema = { type: 'object', properties: { name: { type: 'string' } } };

    await helper.elicit('Provide a name', schema, { timeoutMs: 999_999_999 });

    expect(elicitInput).toHaveBeenCalledWith(expect.any(Object), { timeout: 600000 });
  });

  it('routes malformed schema objects to the alternate handler without RPC', async () => {
    const elicitInput = vi.fn(async () => ({ action: 'accept', content: { ok: true } }));
    const alternate = vi.fn(async () => ({ ok: true, value: 'fallback' }));
    const helper = createElicitationHelper({ elicitInput }, { debug: vi.fn() } as unknown as Logger);
    const malformedSchema = { type: 'object', properties: null } as unknown as ElicitSchema;

    const result = await helper.elicit('Provide a name', malformedSchema, { alternate });

    expect(result).toEqual({ ok: true, value: 'fallback' });
    expect(alternate).toHaveBeenCalledOnce();
    expect(elicitInput).not.toHaveBeenCalled();
  });
});
