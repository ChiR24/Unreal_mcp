import { ProtocolError, type ServerContext } from '@modelcontextprotocol/server';
import { describe, expect, it } from 'vitest';

import { askThroughClient, consentChannel, consentStateCodec, type ConsentChannel } from './gateway-execute-consent.js';

const CAPABILITY = 'asset.delete';
const PARAMS = { path: '/Game/Scratch/SM_Probe' };
const ctx = { mcpReq: { method: 'tools/call' } } as unknown as ServerContext;

function channel(overrides: Partial<ConsentChannel> = {}): ConsentChannel {
  return { canAsk: true, state: undefined, responses: undefined, mint: (state) => consentStateCodec.mint(state, ctx), ...overrides };
}

/** The question, then the verified state a retry carries back (the SDK runs verify before the handler). */
async function asked(): Promise<{ state: unknown; message: string }> {
  const outcome = await askThroughClient(channel(), CAPABILITY, 'elevated', 'manage_asset.delete', PARAMS);
  if (outcome?.kind !== 'ask') throw new Error('expected a question');
  const request = outcome.result.inputRequests?.consent as { method: string; params: { message: string } };
  expect(request.method).toBe('elicitation/create');
  return { state: await consentStateCodec.verify(String(outcome.result.requestState), ctx), message: request.params.message };
}

describe('consent through the client (MCP 2026-07-28)', () => {
  it('asks nobody that cannot be asked', async () => {
    expect(await askThroughClient(undefined, CAPABILITY, 'elevated', 'manage_asset.delete', PARAMS)).toBeUndefined();
    expect(await askThroughClient(channel({ canAsk: false }), CAPABILITY, 'elevated', 'manage_asset.delete', PARAMS)).toBeUndefined();
  });

  it('asks a client that can elicit, naming the call and its target', async () => {
    const { message } = await asked();
    expect(message).toContain('manage_asset.delete (path=/Game/Scratch/SM_Probe)');
    expect(message).toContain('elevated consent');
  });

  it('turns a yes into the grant, with the nonce the plugin burns once', async () => {
    const { state } = await asked();
    const outcome = await askThroughClient(
      channel({ state, responses: { consent: { action: 'accept', content: { confirm: true } } } }),
      CAPABILITY, 'elevated', 'manage_asset.delete', PARAMS
    );
    expect(outcome?.kind).toBe('yes');
    if (outcome?.kind === 'yes') {
      expect(outcome.acknowledge).toBe('elevated');
      expect(outcome.nonce.length).toBeGreaterThan(0);
    }
  });

  it('refuses after a no or an unticked yes, and asks again when the answer is missing', async () => {
    const { state } = await asked();
    const answer = async (responses: Record<string, unknown> | undefined) =>
      (await askThroughClient(channel({ state, responses }), CAPABILITY, 'elevated', 'manage_asset.delete', PARAMS))?.kind;
    expect(await answer({ consent: { action: 'decline' } })).toBe('no');
    expect(await answer({ consent: { action: 'accept', content: { confirm: false } } })).toBe('no');
    expect(await answer(undefined)).toBe('ask');
  });

  it('refuses a verified state minted for another call with -32602', async () => {
    const { state } = await asked();
    const other = askThroughClient(
      channel({ state, responses: { consent: { action: 'accept', content: { confirm: true } } } }),
      CAPABILITY, 'elevated', 'manage_asset.delete', { path: '/Game/Elsewhere' }
    );
    await expect(other).rejects.toBeInstanceOf(ProtocolError);
    await expect(other).rejects.toMatchObject({ code: -32602 });
  });

  it('reads the asking from the per-request envelope only', () => {
    const modern = (elicitation: unknown) => consentChannel({
      mcpReq: {
        method: 'tools/call',
        envelope: { 'io.modelcontextprotocol/clientCapabilities': { elicitation } },
        requestState: () => undefined
      }
    } as unknown as ServerContext).canAsk;
    expect(modern({})).toBe(true);
    expect(modern({ form: {} })).toBe(true);
    expect(modern({ url: {} })).toBe(false);
    expect(modern(undefined)).toBe(false);
    expect(consentChannel({ mcpReq: { method: 'tools/call', requestState: () => undefined } } as unknown as ServerContext).canAsk).toBe(false);
  });
});
