// Consent through the client (MCP 2026-07-28 multi round-trip requests), the TypeScript twin of McpModernConsent on
// the native door. A call whose capability needs a grant and carries none asks the user with a yes/no form, and the
// retry carrying their yes becomes the grant describe would mint. Only a 2026-07-28 request from a client that
// declared form elicitation is asked; everyone else keeps describe's consentGrant.
//
// The requestState is signed with a per-run key and bound to the method (the SDK verifies both, and the expiry,
// before the handler runs); it carries a digest of the exact call and a nonce the plugin's consent ledger burns once.

import { randomBytes, randomUUID } from 'node:crypto';
import {
  CLIENT_CAPABILITIES_META_KEY,
  ProtocolError,
  ProtocolErrorCode,
  acceptedContent,
  createRequestStateCodec,
  inputRequired,
  inputResponse,
  type InputRequiredResult,
  type ServerContext
} from '@modelcontextprotocol/server';
import { z } from 'zod';

import { isRecord } from '../../utils/validation/type-guards.js';
import { canonicalFingerprint } from './gateway-execute-idempotency.js';

type Acknowledge = 'explicit' | 'elevated';

const ConsentStateSchema = z.object({
  capability: z.string(),
  acknowledge: z.enum(['explicit', 'elevated']),
  digest: z.string(),
  nonce: z.string().min(1)
});
type ConsentState = z.infer<typeof ConsentStateSchema>;

/** Signs the question's requestState; its verify is the server's `requestState.verify`, run before the handler. */
export const consentStateCodec = createRequestStateCodec<ConsentState>({
  key: randomBytes(32),
  ttlSeconds: 600,
  bind: (ctx) => ctx.mcpReq.method
});

const INPUT_KEY = 'consent';
const ConfirmSchema = z.object({ confirm: z.boolean() });

/** What a tools/call request says about asking its user. */
export interface ConsentChannel {
  readonly canAsk: boolean;
  readonly state: unknown;
  readonly responses: Record<string, unknown> | undefined;
  /** Answers the SDK dropped as not bare responses (a `{method, result}` wrapper). */
  readonly dropped?: readonly string[];
  mint(state: ConsentState): Promise<string>;
}

export function consentChannel(ctx: ServerContext): ConsentChannel {
  // Only a 2026-07-28 request carries the envelope; an elicitation capability naming no mode means form.
  const envelope: unknown = ctx.mcpReq.envelope;
  const capabilities = isRecord(envelope) ? envelope[CLIENT_CAPABILITIES_META_KEY] : undefined;
  const elicitation = isRecord(capabilities) ? capabilities.elicitation : undefined;
  return {
    canAsk: isRecord(elicitation) && (elicitation.form !== undefined || elicitation.url === undefined),
    state: ctx.mcpReq.requestState(),
    responses: ctx.mcpReq.inputResponses,
    dropped: ctx.mcpReq.droppedInputResponseKeys,
    mint: (state) => consentStateCodec.mint(state, ctx)
  };
}

export type ConsentOutcome =
  | { readonly kind: 'ask'; readonly result: InputRequiredResult }
  | { readonly kind: 'yes'; readonly acknowledge: Acknowledge; readonly nonce: string }
  | { readonly kind: 'no'; readonly message: string };

/** The scalar arguments the call is about to act on, for the question. */
function describeArguments(params: Record<string, unknown>): string {
  const parts = Object.entries(params)
    .filter(([, value]) => typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean')
    .slice(0, 6)
    .map(([key, value]) => `${key}=${String(value).slice(0, 80)}`);
  return parts.length === 0 ? '' : ` (${parts.join(', ')})`;
}

/**
 * Undefined when the request cannot ask (not 2026-07-28, or no form elicitation): refuse as before. A verified state
 * minted for another call is refused outright, as the native door refuses it.
 */
export async function askThroughClient(
  channel: ConsentChannel | undefined,
  capabilityId: string,
  consent: Acknowledge,
  target: string,
  params: Record<string, unknown>
): Promise<ConsentOutcome | undefined> {
  if (channel === undefined) return undefined;
  const digest = canonicalFingerprint(capabilityId, params);
  if (channel.state !== undefined) {
    const state = ConsentStateSchema.safeParse(channel.state);
    if (!state.success || state.data.capability !== capabilityId || state.data.digest !== digest) {
      throw new ProtocolError(ProtocolErrorCode.InvalidParams, 'Invalid or expired requestState', { reason: 'invalid_request_state' });
    }
    // An answer the SDK dropped as wrapped read as no answer, so the question went out again every round until the
    // round cap failed the call: a client that always wraps its answers gets the other way through instead.
    if (channel.dropped?.includes(INPUT_KEY) === true) {
      return { kind: 'no', message: `The client answered the consent question for ${target} in a shape this server cannot read (a wrapped response), so it did not run. Run describe and pass its consentGrant as consent instead.` };
    }
    const answer = inputResponse(channel.responses, INPUT_KEY);
    if (answer.kind !== 'missing') {
      if (acceptedContent(channel.responses, INPUT_KEY, ConfirmSchema)?.confirm === true) {
        return { kind: 'yes', acknowledge: state.data.acknowledge, nonce: state.data.nonce };
      }
      return { kind: 'no', message: `The user did not allow ${target} when the client asked, so it did not run. Do not run it again unless they ask for it.` };
    }
  }
  if (!channel.canAsk) return undefined;
  const requestState = await channel.mint({ capability: capabilityId, acknowledge: consent, digest, nonce: randomUUID() });
  return {
    kind: 'ask',
    result: inputRequired({
      inputRequests: {
        [INPUT_KEY]: inputRequired.elicit({
          message: `Unreal Engine asks before it runs ${target}${describeArguments(params)}: this capability needs ${consent} consent. Allow it?`,
          requestedSchema: {
            type: 'object',
            properties: { confirm: { type: 'boolean', title: 'Allow this call', default: false } },
            required: ['confirm']
          }
        })
      },
      requestState
    })
  };
}
