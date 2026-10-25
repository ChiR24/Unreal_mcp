// Consent asked through the client (MCP 2026-07-28) on the TypeScript door: a call that needs a grant and carries
// none answers an input_required question instead of CONSENT_REQUIRED, and the retry carrying the user's yes runs
// the call with the grant (and its single-use nonce) as the envelope sibling the plugin re-validates.

import { isInputRequiredResult, type ServerContext } from '@modelcontextprotocol/server';
import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { consentStateCodec, type ConsentChannel } from '../../../src/server/gateway/gateway-execute-consent.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

const ctx = { mcpReq: { method: 'tools/call' } } as unknown as ServerContext;
const CALL = { operation: 'execute', tool: 'manage_asset', action: 'delete', params: { path: '/Game/Scratch/SM_Probe' } };
const YES = { consent: { action: 'accept', content: { confirm: true } } };

function context(sent: unknown[], consent: Partial<ConsentChannel> | undefined): GatewayContext {
  const base = gatewayContext({
    isConnected: () => true,
    sendAutomationRequest: async (_action, _payload, options) => {
      sent.push(options?.consent);
      return { success: true };
    },
    getAuthority: () => ({ scopes: ['admin'] })
  }, 'consent-through-client');
  if (consent === undefined) return base;
  return { ...base, consent: { canAsk: true, state: undefined, responses: undefined, mint: (state) => consentStateCodec.mint(state, ctx), ...consent } };
}

async function question(sent: unknown[]): Promise<{ nonce: string }> {
  const first = await handleUnrealGatewayCall(CALL, context(sent, {}));
  expect(isInputRequiredResult(first), JSON.stringify(first)).toBe(true);
  return await consentStateCodec.verify(String(first.requestState), ctx) as { nonce: string };
}

describe('consent through the client on the TypeScript door', () => {
  it('asks before anything is dispatched, then runs the call with the grant and its nonce once the user says yes', async () => {
    const sent: unknown[] = [];
    const state = await question(sent);
    expect(sent, 'the question dispatches nothing').toEqual([]);
    await handleUnrealGatewayCall(CALL, context(sent, { state, responses: YES }));
    expect(sent).toEqual([{ capability: 'asset.delete', acknowledge: 'elevated', nonce: state.nonce }]);
  });

  it('refuses after a no without handing over describe, whose grant would run what the user refused', async () => {
    const sent: unknown[] = [];
    const state = await question(sent);
    const refused = await handleUnrealGatewayCall(CALL, context(sent, { state, responses: { consent: { action: 'decline' } } }));
    expect(refused.errorCode).toBe('CONSENT_REQUIRED');
    expect(String(refused.message)).toContain('did not allow manage_asset.delete');
    expect(refused.nextCall).toBeUndefined();
    expect(sent).toEqual([]);
  });

  it('an older client keeps the CONSENT_REQUIRED refusal and its describe nextCall', async () => {
    const sent: unknown[] = [];
    const refused = await handleUnrealGatewayCall(CALL, context(sent, undefined));
    expect(refused.errorCode).toBe('CONSENT_REQUIRED');
    expect((refused.nextCall as Record<string, unknown> | undefined)?.operation).toBe('describe');
    expect(sent).toEqual([]);
  });
});
