// src/server/mcp-primitives/completions/completion-types.ts
// Task 33 primitive: bounded MCP completion types. Single TypeScript source of
// truth for the completion primitive shared by the slot registry, ranking,
// provider, and (later) Task 37 protocol wiring. It carries NO transport wiring
// and NO session-lifecycle edits, executes NOTHING, and scans NO editor. It
// consumes the Task 35 C3 SessionCapabilityProfile read-only. The native mirror
// is Private/MCP/Primitives/McpCompletionProvider.{h,cpp}.

// --- MCP completion/complete wire shapes (mirror the SDK schema) ---

/** A reference to a prompt whose argument is being completed. */
export interface PromptReference {
  readonly type: 'ref/prompt';
  readonly name: string;
}

/** A reference to a resource template whose variable is being completed. */
export interface ResourceReference {
  readonly type: 'ref/resource';
  readonly uri: string;
}

export type CompletionReference = PromptReference | ResourceReference;

/** The single argument/variable being completed and the text before the cursor. */
export interface CompletionArgument {
  readonly name: string;
  readonly value: string;
}

/** Previously-resolved sibling arguments (MCP context.arguments). */
export interface CompletionContext {
  readonly arguments?: Readonly<Record<string, string>>;
}

/**
 * A completion request as the pure provider sees it: the MCP ref, the argument
 * (name plus the prefix before the cursor), and optional context. The session id
 * is passed to the provider SEPARATELY and explicitly; it is never read from a
 * central session object here.
 */
export interface CompletionRequest {
  readonly ref: CompletionReference;
  readonly argument: CompletionArgument;
  readonly context?: CompletionContext;
}

/**
 * The MCP completion payload. `values` is the bounded, ranked slice actually
 * returned; `total` is the full matched count before the item cap; `hasMore` is
 * true when the item or byte budget truncated the matched set.
 */
export interface CompletionResult {
  readonly values: readonly string[];
  readonly total: number;
  readonly hasMore: boolean;
}

// --- Bounded budgets ---

/** MCP hard cap: a completion response never carries more than 100 values. */
export const MAX_COMPLETION_ITEMS = 100;
/** Serialized byte budget for the returned values (defensive, below the cap). */
export const MAX_COMPLETION_BYTES = 8192;
/** Longest prefix we rank against; a longer prefix is refused, never scanned. */
export const MAX_PREFIX_LENGTH = 128;

// --- Candidate and slot model ---

/** What a candidate is (drives display and telemetry, not safety). */
export const CANDIDATE_KINDS = ['capability', 'legacy-id', 'enum', 'project-handle'] as const;
export type CandidateKind = (typeof CANDIDATE_KINDS)[number];

/**
 * One completion candidate. `value` is what the client receives. `capabilityId`
 * is set for capability/legacy candidates so a capability-scoped slot can be
 * filtered by the session enabled-capability set (a legacy id maps to its
 * canonical capability id).
 */
export interface CompletionCandidate {
  readonly value: string;
  readonly kind: CandidateKind;
  readonly capabilityId?: string;
}

/** The candidate pool a completable slot draws from. */
export const SLOT_KINDS = ['capability', 'enum', 'project-handle'] as const;
export type SlotKind = (typeof SLOT_KINDS)[number];

/**
 * A resolved completable slot: the ref type, the normalized ref id (prompt name
 * or resource uri template), the argument/variable name, the candidate pool
 * kind, and whether the pool is filtered by the session enabled capabilities.
 */
export interface CompletionSlot {
  readonly refType: CompletionReference['type'];
  readonly refId: string;
  readonly argumentName: string;
  readonly kind: SlotKind;
  readonly capabilityScoped: boolean;
}

// --- Injected candidate source (read-only, static) ---

/**
 * Read-only source of completion candidates. Every method returns bounded,
 * in-memory data derived from the generated registry/migration/schema and safe
 * caches: NEVER a live editor scan and NEVER a raw filesystem path. Task 37
 * injects the concrete source; tests inject deterministic fixtures.
 */
export interface CompletionCandidateSource {
  /** Canonical capability ids plus legacy migration ids, each tagged. */
  capabilityCandidates(): readonly CompletionCandidate[];
  /** Enum/schema values for one enum slot, or [] when the slot has none. */
  enumCandidates(slot: CompletionSlot): readonly CompletionCandidate[];
  /** Safe cached project handles for one handle slot, or [] when empty. */
  projectHandleCandidates(slot: CompletionSlot): readonly CompletionCandidate[];
}

// --- Safe-empty guidance ---

/** Stable codes explaining why a request yielded no safe candidate. */
export const COMPLETION_GUIDANCE_CODES = {
  SECRET_FIELD: 'COMPLETION_SECRET_FIELD',
  DESTRUCTIVE_FIELD: 'COMPLETION_DESTRUCTIVE_FIELD',
  UNBOUNDED_PREFIX: 'COMPLETION_UNBOUNDED_PREFIX',
  UNBOUNDED_PATH: 'COMPLETION_UNBOUNDED_PATH',
  UNAVAILABLE: 'COMPLETION_UNAVAILABLE',
  NO_MATCH: 'COMPLETION_NO_MATCH',
} as const;

export type CompletionGuidanceCode =
  (typeof COMPLETION_GUIDANCE_CODES)[keyof typeof COMPLETION_GUIDANCE_CODES];

/**
 * Bounded, executable guidance for the no-safe-candidate case. `nextCall` is a
 * tiny gateway operation (mirrors the fallback-pointer shape): never a schema or
 * knowledge dump. A refusal (secret/destructive/unbounded) carries no nextCall
 * that could echo the unsafe value back.
 */
export interface CompletionGuidance {
  readonly code: CompletionGuidanceCode;
  readonly reason: string;
  readonly nextCall?: Readonly<Record<string, unknown>>;
}

/**
 * The provider internal outcome: the MCP wire payload plus optional guidance. A
 * safe-empty outcome always carries guidance; a matched outcome never does.
 * Task 37 maps `completion` to the wire and may attach `guidance` to `_meta`.
 */
export interface CompletionOutcome {
  readonly completion: CompletionResult;
  readonly guidance?: CompletionGuidance;
}

/** The empty completion payload reused by every safe-empty outcome. */
export const EMPTY_COMPLETION: CompletionResult = Object.freeze({
  values: Object.freeze([]) as readonly string[],
  total: 0,
  hasMore: false,
});
