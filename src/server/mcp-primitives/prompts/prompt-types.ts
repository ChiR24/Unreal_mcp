// src/server/mcp-primitives/prompts/prompt-types.ts
// Task 32 primitive C1: user-selected workflow prompt types.
//
// This module is the single TypeScript source of truth for the prompt primitive
// shared by the prompt catalog (Task 32) and, later, protocol wiring (Task 37).
// It carries NO transport wiring, NO tool execution, and NO stored state; it
// defines the branded version type, the closed six-workflow id allowlist, the
// strict typed-argument kinds, the immutable workflow/step shapes, and the
// injected reference validator. The native mirror is
// `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpPromptCatalog.h`.

/**
 * A numeric, monotonically non-decreasing prompt definition version. Branded so
 * a raw `number` cannot be mistaken for a version at a call boundary.
 */
export type PromptVersion = number & { readonly __brand: 'PromptVersion' };

/** The version every workflow prompt starts at. */
export const INITIAL_PROMPT_VERSION: PromptVersion = 1 as PromptVersion;

/**
 * Parse an arbitrary number into a `PromptVersion`. Versions are integers >= 1;
 * anything else is a programming error and is rejected rather than coerced.
 */
export function asPromptVersion(value: number): PromptVersion {
  if (!Number.isInteger(value) || value < 1) {
    throw new RangeError(`Invalid prompt version: ${String(value)} (expected integer >= 1)`);
  }
  return value as PromptVersion;
}

/**
 * The closed allowlist of user-selectable workflow prompt names. Discovery-first
 * workflows only; there is no free-form or dynamically generated prompt.
 */
export const WORKFLOW_PROMPT_IDS = [
  'inspect-fix',
  'asset-import',
  'level-build',
  'blueprint-edit',
  'validation',
  'sequence-render',
] as const;

export type WorkflowPromptId = (typeof WORKFLOW_PROMPT_IDS)[number];

const WORKFLOW_PROMPT_ID_SET: ReadonlySet<string> = new Set(WORKFLOW_PROMPT_IDS);

/** Narrow an arbitrary string to a `WorkflowPromptId` from the closed allowlist. */
export function isWorkflowPromptId(name: string): name is WorkflowPromptId {
  return WORKFLOW_PROMPT_ID_SET.has(name);
}

/** The strict typed-argument kinds a workflow prompt argument can declare. */
export const PROMPT_ARGUMENT_KINDS = [
  'content-path',
  'object-path',
  'identifier',
  'enum',
  'engine-version',
  'text',
] as const;

export type PromptArgumentKind = (typeof PROMPT_ARGUMENT_KINDS)[number];

const PROMPT_ARGUMENT_KIND_SET: ReadonlySet<string> = new Set(PROMPT_ARGUMENT_KINDS);

/** Narrow an arbitrary string to a `PromptArgumentKind`. */
export function isPromptArgumentKind(value: string): value is PromptArgumentKind {
  return PROMPT_ARGUMENT_KIND_SET.has(value);
}

/**
 * A strictly typed prompt argument. `kind` drives boundary validation; `allowed`
 * is required only for `enum`. Arguments are never secrets and are never
 * host-filesystem paths — the validators enforce that.
 */
export interface PromptArgumentSpec {
  readonly name: string;
  readonly description: string;
  readonly required: boolean;
  readonly kind: PromptArgumentKind;
  readonly allowed?: readonly string[];
  readonly example: string;
}

/**
 * One step of a workflow: exactly one canonical capability, the parent tool and
 * legacy action used to `describe` it, an optional Task 31 resource to read for
 * confirmation, and a human safety note. `capabilityId` must exist in the
 * generated canonical registry; `resourceUri` must be a Task 31 approved uri.
 */
export interface PromptStep {
  readonly summary: string;
  readonly capabilityId: string;
  readonly parentTool: string;
  readonly action: string;
  readonly resourceUri?: string;
  readonly safety: string;
}

/** A versioned, immutable user-selected workflow prompt definition. */
export interface WorkflowPrompt {
  readonly id: WorkflowPromptId;
  readonly version: PromptVersion;
  readonly title: string;
  readonly description: string;
  readonly arguments: readonly PromptArgumentSpec[];
  readonly steps: readonly PromptStep[];
}

/** The MCP-visible argument triple returned by `prompts/list` (no internal kind). */
export interface ListPromptArgument {
  readonly name: string;
  readonly description: string;
  readonly required: boolean;
}

/** A `prompts/list` entry: MCP prompt metadata plus the definition version. */
export interface ListPromptEntry {
  readonly name: WorkflowPromptId;
  readonly version: PromptVersion;
  readonly title: string;
  readonly description: string;
  readonly arguments: readonly ListPromptArgument[];
}

/** A single user-role text message — the only content type a prompt emits. */
export interface PromptTextMessage {
  readonly role: 'user';
  readonly content: { readonly type: 'text'; readonly text: string };
}

/** The `prompts/get` result: a bounded description plus one text message. */
export interface GetPromptOutput {
  readonly description: string;
  readonly version: PromptVersion;
  readonly messages: readonly PromptTextMessage[];
}

/**
 * Injected read-only reference source. `getPrompt` asks whether each referenced
 * capability id and resource uri still exists so a stale/regenerated registry
 * fails closed instead of emitting a dangling reference. Task 37 supplies the
 * live validator; tests inject deterministic sets.
 */
export interface PromptReferenceValidator {
  capabilityExists(capabilityId: string): boolean;
  resourceExists(resourceUri: string): boolean;
}
