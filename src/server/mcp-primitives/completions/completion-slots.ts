import { HOST_PATH_PATTERN, SECRET_NAME_PATTERN, isTraversalPath } from '../../../utils/paths/content-path-policy.js';
// src/server/mcp-primitives/completions/completion-slots.ts
// Task 33: the closed registry of completable slots and the safety gate. A slot
// ties one (ref, argument) pair to a bounded candidate pool kind; anything not
// listed is not completable and yields UNAVAILABLE rather than an editor scan.
// The safety gate refuses secrets, destructive confirmations, and raw host
// paths by name/value, independent of whether the pair is a known slot. Native
// mirror: Private/MCP/Primitives/McpCompletionProvider.{h,cpp}.

import {
  COMPLETION_GUIDANCE_CODES,
  type CompletionGuidanceCode,
  type CompletionReference,
  type CompletionSlot,
} from './completion-types.js';

/**
 * The closed registry of completable slots. Resource ids are the Task 31
 * template URIs; prompt ids are Task 32 workflow names. Capability slots are
 * capability-scoped so they are filtered by the session enabled-capability set;
 * enum and project-handle slots draw from bounded static/cached pools.
 */
export const COMPLETION_SLOTS: readonly CompletionSlot[] = [
  { refType: 'ref/resource', refId: 'ue://capability/{capabilityId}', argumentName: 'capabilityId', kind: 'capability', capabilityScoped: true },
  { refType: 'ref/resource', refId: 'ue://knowledge/{engineVersion}/{topic}', argumentName: 'engineVersion', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://knowledge/{engineVersion}/{topic}', argumentName: 'topic', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://object/{objectPath}', argumentName: 'objectPath', kind: 'project-handle', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://asset/{assetPath}', argumentName: 'assetPath', kind: 'project-handle', capabilityScoped: false },
  { refType: 'ref/prompt', refId: 'asset-import', argumentName: 'sourceFormat', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/prompt', refId: 'sequence-render', argumentName: 'outputFormat', kind: 'enum', capabilityScoped: false },
];

/** The normalized ref id: a prompt name or a resource template uri. */
export function refIdOf(ref: CompletionReference): string {
  return ref.type === 'ref/prompt' ? ref.name : ref.uri;
}

/**
 * Resolve a (ref, argument) pair to its completable slot, or undefined when the
 * pair is not in the closed registry. Deterministic: the registry is a fixed
 * table and the first exact match wins.
 */
export function resolveSlot(ref: CompletionReference, argumentName: string): CompletionSlot | undefined {
  const id = refIdOf(ref);
  return COMPLETION_SLOTS.find(
    (slot) => slot.refType === ref.type && slot.refId === id && slot.argumentName === argumentName,
  );
}

// --- Safety classification ---

// Argument names that gate a destructive action; a confirmation is typed by the
// user, never auto-completed.
const DESTRUCTIVE_NAME_PATTERN = /(confirm|force|overwrite|purge|wipe|destroy)/;

const isTraversal = isTraversalPath;

/**
 * Classify an argument name+value as unsafe to complete, or undefined when it is
 * safe. Runs on the NAME (secret/destructive) and the VALUE (raw host path),
 * independent of whether the pair is a known slot, so a secret field is refused
 * even on an unknown ref.
 */
export function classifyUnsafe(argumentName: string, value: string): CompletionGuidanceCode | undefined {
  const lower = argumentName.toLowerCase();
  if (SECRET_NAME_PATTERN.test(lower)) return COMPLETION_GUIDANCE_CODES.SECRET_FIELD;
  if (DESTRUCTIVE_NAME_PATTERN.test(lower)) return COMPLETION_GUIDANCE_CODES.DESTRUCTIVE_FIELD;
  if (HOST_PATH_PATTERN.test(value) || isTraversal(value)) return COMPLETION_GUIDANCE_CODES.UNBOUNDED_PATH;
  return undefined;
}
