// src/server/mcp-primitives/completions/index.ts
// Task 33: public surface of the bounded completion primitive. This barrel is
// the single import point for Task 37 protocol wiring; it exposes the pure
// provider, the concrete static candidate source, the slot registry and safety
// gate, the ranking helpers, and the primitive types. It adds NO behavior and
// performs NO side effects on import.

export { complete } from './completion-provider.js';
export { createStaticCompletionSource } from './completion-sources.js';
export { applyBudget, rankCandidates } from './completion-ranking.js';
export {
  COMPLETION_SLOTS,
  classifyUnsafe,
  refIdOf,
  resolveSlot,
} from './completion-slots.js';
export {
  COMPLETION_GUIDANCE_CODES,
  EMPTY_COMPLETION,
  MAX_COMPLETION_BYTES,
  MAX_COMPLETION_ITEMS,
  MAX_PREFIX_LENGTH,
  type CandidateKind,
  type CompletionArgument,
  type CompletionCandidate,
  type CompletionCandidateSource,
  type CompletionContext,
  type CompletionGuidance,
  type CompletionGuidanceCode,
  type CompletionOutcome,
  type CompletionReference,
  type CompletionRequest,
  type CompletionResult,
  type CompletionSlot,
  type PromptReference,
  type ResourceReference,
  type SlotKind,
} from './completion-types.js';
