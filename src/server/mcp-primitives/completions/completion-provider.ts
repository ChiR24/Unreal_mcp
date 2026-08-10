// src/server/mcp-primitives/completions/completion-provider.ts
// Task 33: the pure completion provider. Given a completion request, an explicit
// session id, a Task 35 C3 SessionCapabilityProfile, and an injected read-only
// candidate source, it returns a bounded, deterministic, ranked outcome or a
// safe-empty outcome with exact guidance. It executes NOTHING, scans NO editor,
// and never suggests a secret, a destructive confirmation, a raw filesystem
// path, or a capability disabled for the session. Task 37 wires this into the
// `completion/complete` method. Native mirror: McpCompletionProvider.{h,cpp}.

import type { SessionCapabilityProfile } from '../session-capability-profile.js';
import { applyBudget, rankCandidates } from './completion-ranking.js';
import { classifyUnsafe, resolveSlot } from './completion-slots.js';
import {
  COMPLETION_GUIDANCE_CODES,
  EMPTY_COMPLETION,
  MAX_PREFIX_LENGTH,
  type CompletionCandidate,
  type CompletionCandidateSource,
  type CompletionGuidance,
  type CompletionGuidanceCode,
  type CompletionOutcome,
  type CompletionRequest,
  type CompletionSlot,
} from './completion-types.js';

function assertNever(value: never): never {
  throw new Error(`Unhandled completion value: ${String(value)}`);
}

// Bounded guidance for a safe-empty outcome. A refusal (secret/destructive/
// unbounded) never echoes the value and carries no nextCall; a discovery case
// points at exactly one gateway operation.
function guidanceFor(code: CompletionGuidanceCode): CompletionGuidance {
  switch (code) {
    case COMPLETION_GUIDANCE_CODES.SECRET_FIELD:
      return { code, reason: 'Completion never suggests secrets or tokens; supply this value yourself.' };
    case COMPLETION_GUIDANCE_CODES.DESTRUCTIVE_FIELD:
      return { code, reason: 'Completion never fills a destructive confirmation; type it yourself to confirm intent.' };
    case COMPLETION_GUIDANCE_CODES.UNBOUNDED_PATH:
      return { code, reason: 'Completion never suggests raw filesystem paths; use a /Game content path or a cached handle.' };
    case COMPLETION_GUIDANCE_CODES.UNBOUNDED_PREFIX:
      return { code, reason: `Prefix exceeds ${String(MAX_PREFIX_LENGTH)} characters; narrow it before requesting completion.` };
    case COMPLETION_GUIDANCE_CODES.UNAVAILABLE:
      return { code, reason: 'No bounded completion source for this field; discover values through the unreal gateway.', nextCall: { operation: 'search' } };
    case COMPLETION_GUIDANCE_CODES.NO_MATCH:
      return { code, reason: 'No safe candidate matched; discover values through the unreal gateway search operation.', nextCall: { operation: 'search' } };
    default:
      return assertNever(code);
  }
}

function safeEmpty(code: CompletionGuidanceCode): CompletionOutcome {
  return { completion: EMPTY_COMPLETION, guidance: guidanceFor(code) };
}

// The bounded candidate pool for a slot. Every branch reads only injected
// static/cached data; none scans the editor.
function poolForSlot(slot: CompletionSlot, source: CompletionCandidateSource): readonly CompletionCandidate[] {
  switch (slot.kind) {
    case 'capability':
      return source.capabilityCandidates();
    case 'enum':
      return source.enumCandidates(slot);
    case 'project-handle':
      return source.projectHandleCandidates(slot);
    default:
      return assertNever(slot.kind);
  }
}

/**
 * Complete one argument value. The order is deliberate and fail-closed:
 * 1. an unbounded prefix is refused before any pool work (cheapest guard);
 * 2. secrets, destructive confirmations, and raw host paths are refused by
 *    name/value, even on an unknown ref;
 * 3. an unknown (ref, argument) pair is UNAVAILABLE (never an editor scan);
 * 4. the bounded pool is drawn, capability-scoped slots are filtered by the
 *    session enabled-capability set, then ranked and capped to the budgets;
 * 5. an empty ranked set returns NO_MATCH guidance, not a misleading success.
 */
export function complete(
  request: CompletionRequest,
  sessionId: string,
  profile: SessionCapabilityProfile,
  source: CompletionCandidateSource,
): CompletionOutcome {
  const { argument } = request;

  if (argument.value.length > MAX_PREFIX_LENGTH) {
    return safeEmpty(COMPLETION_GUIDANCE_CODES.UNBOUNDED_PREFIX);
  }

  const unsafe = classifyUnsafe(argument.name, argument.value);
  if (unsafe !== undefined) {
    return safeEmpty(unsafe);
  }

  const slot = resolveSlot(request.ref, argument.name);
  if (slot === undefined) {
    return safeEmpty(COMPLETION_GUIDANCE_CODES.UNAVAILABLE);
  }

  let pool = poolForSlot(slot, source);
  if (slot.capabilityScoped) {
    const enabled = profile.enabledCapabilityIds(sessionId);
    pool = pool.filter((candidate) => candidate.capabilityId === undefined || enabled.has(candidate.capabilityId));
  }

  const ranked = rankCandidates(pool, argument.value);
  if (ranked.length === 0) {
    return safeEmpty(COMPLETION_GUIDANCE_CODES.NO_MATCH);
  }
  return { completion: applyBudget(ranked) };
}
