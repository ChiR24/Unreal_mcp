// tests/unit/mcp-primitives/prompts-completions-native-fixture.ts
//
// Task 38 lane B — EXECUTABLE native-behavior fixture (oracle) for cross-transport
// prompts/list, prompts/get, and completion/complete parity.
//
// This is NOT a source-text contract. It is a deterministic, executable,
// INDEPENDENT reimplementation of the NATIVE C++ `/mcp` transport's *normalized*
// observable behavior for the prompt and completion primitives. A Vitest suite
// drives it through the same scenarios as the TypeScript production primitives and
// compares SEMANTICS (normalized results / error codes), never HTTP/SSE framing
// and never grepped C++ text, so the parity proof is executable behavior. Every
// rule below is grounded in a native source location verified first-hand against
// the plugin tree at HEAD (dev branch).
//
// GROUNDING MAP (plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP):
//   Six workflow prompt ids + titles        Primitives/McpPromptCatalog.cpp:14-62
//   Prompt secret-name guard (data only)    Primitives/McpPromptCatalog.cpp:69-85
//   Completable slot table (7)              Primitives/McpCompletionProvider.cpp:12-24
//   Enum value sets                         Primitives/McpCompletionProvider.cpp:38-57
//   Safety gate (secret/destructive/path)   Primitives/McpCompletionProvider.cpp:158-174
//   Deterministic ranking (tier ladder)     Primitives/McpCompletionProvider.cpp:141-204
//   Item/byte budget cap                    Primitives/McpCompletionProvider.cpp:206-223
//   Budgets + guidance codes                Primitives/McpCompletionProvider.h:17-31
//   Fail-closed orchestration               Primitives/McpCompletionProvider.cpp:235-277
//
// REMEDIATION (Task 38): the two prompt/completion divergences the RED gates named
// are now CLOSED in native production, and this oracle models the remediated
// transport:
//   * PROMPT SURFACE — prompts/list now delegates to McpBuildPromptListEntries
//     (full name/title/description/arguments) and prompts/get to
//     McpRenderWorkflowPrompt (McpPromptRender.cpp), which renders the byte-identical
//     multi-step body and enforces secret / unknown / missing / invalid / too-large
//     argument validation (McpPromptArgumentValidation.cpp). The independent prompt
//     model lives in ./prompts-completions-native-prompts.ts.
//   * COMPLETION POOLS — completion/complete now injects the real capability pool
//     (McpCapabilityCompletionPool), the class-alias project-handle pool
//     (McpProjectHandleCompletionPool), and the session enabled-capability set
//     (McpEnabledCapabilityIds) from McpCompletionPools.cpp, so capability and
//     project-handle slots return ranked candidates instead of NO_MATCH.
// The completion ranking/budget/refusal logic below is unchanged (already a faithful
// port); only the pool wiring moved from empty to populated.

/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

// The prompt surface (list/get/render/validate) is modeled in the sibling oracle;
// re-exported here so the parity and baseline suites keep a single import surface.
export {
  nativePromptsGet,
  nativePromptsList,
  type NativePromptGetResult,
  type NativePromptListEntry,
} from './prompts-completions-native-prompts.js';

// ---------------------------------------------------------------------------
// Grounded native constants (independent of the TS modules under test).
// ---------------------------------------------------------------------------

/** McpPromptCatalog.cpp:57-62 — the closed six-workflow id allowlist, in order. */
export const NATIVE_WORKFLOW_PROMPT_IDS = [
  'inspect-fix',
  'asset-import',
  'level-build',
  'blueprint-edit',
  'validation',
  'sequence-render',
] as const;

export type NativeWorkflowPromptId = (typeof NATIVE_WORKFLOW_PROMPT_IDS)[number];

/** McpCompletionProvider.h:17-19 — bounded budgets. */
export const NATIVE_MAX_COMPLETION_ITEMS = 100;
export const NATIVE_MAX_COMPLETION_BYTES = 8192;
export const NATIVE_MAX_COMPLETION_PREFIX_LENGTH = 128;

/** McpCompletionProvider.h:25-30 — the stable guidance codes for a safe-empty outcome. */
export const NATIVE_COMPLETION_GUIDANCE = {
  SECRET_FIELD: 'COMPLETION_SECRET_FIELD',
  DESTRUCTIVE_FIELD: 'COMPLETION_DESTRUCTIVE_FIELD',
  UNBOUNDED_PREFIX: 'COMPLETION_UNBOUNDED_PREFIX',
  UNBOUNDED_PATH: 'COMPLETION_UNBOUNDED_PATH',
  UNAVAILABLE: 'COMPLETION_UNAVAILABLE',
  NO_MATCH: 'COMPLETION_NO_MATCH',
} as const;

export type NativeCompletionGuidanceCode =
  (typeof NATIVE_COMPLETION_GUIDANCE)[keyof typeof NATIVE_COMPLETION_GUIDANCE];

/** McpCompletionProvider.cpp:38-57 — the bounded enum value sets. */
export const NATIVE_ENUM_SETS: Readonly<Record<string, readonly string[]>> = {
  engineVersion: ['5.0', '5.1', '5.2', '5.3', '5.4', '5.5', '5.6', '5.7', '5.8'],
  // The keys the knowledge table actually serves. The previous list shared zero
  // values with it, so every completed topic answered RESOURCE_NOT_FOUND.
  topic: ['gateway', 'paths', 'resources', 'safety', 'transports'],
  sourceFormat: ['fbx', 'obj', 'gltf', 'png', 'wav'],
  outputFormat: ['png', 'jpeg', 'exr', 'custom'],
};

/** McpCompletionProvider.cpp:162-169 — the safety-gate name fragments. */
export const NATIVE_SECRET_FRAGMENTS = [
  'token', 'secret', 'password', 'passwd', 'apikey', 'api_key',
  'credential', 'privatekey', 'private_key', 'bearer', 'auth',
] as const;
export const NATIVE_DESTRUCTIVE_FRAGMENTS = [
  'confirm', 'force', 'overwrite', 'purge', 'wipe', 'destroy',
] as const;

export interface NativeCompletionSlot {
  readonly refType: 'ref/prompt' | 'ref/resource';
  readonly refId: string;
  readonly argumentName: string;
  readonly kind: 'capability' | 'enum' | 'project-handle';
  readonly capabilityScoped: boolean;
}

/** McpCompletionProvider.cpp:12-24 — the closed completable-slot registry. */
export const NATIVE_COMPLETION_SLOTS: readonly NativeCompletionSlot[] = [
  { refType: 'ref/resource', refId: 'ue://capability/{capabilityId}', argumentName: 'capabilityId', kind: 'capability', capabilityScoped: true },
  { refType: 'ref/resource', refId: 'ue://knowledge/{engineVersion}/{topic}', argumentName: 'engineVersion', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://knowledge/{engineVersion}/{topic}', argumentName: 'topic', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://object/{objectPath}', argumentName: 'objectPath', kind: 'project-handle', capabilityScoped: false },
  { refType: 'ref/resource', refId: 'ue://asset/{assetPath}', argumentName: 'assetPath', kind: 'project-handle', capabilityScoped: false },
  { refType: 'ref/prompt', refId: 'asset-import', argumentName: 'sourceFormat', kind: 'enum', capabilityScoped: false },
  { refType: 'ref/prompt', refId: 'sequence-render', argumentName: 'outputFormat', kind: 'enum', capabilityScoped: false },
];

// ---------------------------------------------------------------------------
// Native completion pure logic — a faithful port of McpCompletionProvider.cpp.
// ---------------------------------------------------------------------------

export interface NativeCompletionCandidate {
  readonly value: string;
  readonly kind: 'capability' | 'legacy-id' | 'enum' | 'project-handle';
  readonly capabilityId?: string;
}

export interface NativeCompletionResult {
  readonly values: readonly string[];
  readonly total: number;
  readonly hasMore: boolean;
}

export interface NativeCompletionOutcome {
  readonly completion: NativeCompletionResult;
  readonly guidanceCode: NativeCompletionGuidanceCode | null;
}

const utf8Len = (value: string): number => Buffer.byteLength(value, 'utf8');

/** McpResolveCompletionSlot (McpCompletionProvider.cpp:26-36). */
export function nativeResolveSlot(
  refType: string,
  refId: string,
  argumentName: string,
): NativeCompletionSlot | undefined {
  return NATIVE_COMPLETION_SLOTS.find(
    (slot) => slot.refType === refType && slot.refId === refId && slot.argumentName === argumentName,
  );
}

/** McpCompletionEnumValues (McpCompletionProvider.cpp:38-57). */
export function nativeEnumValues(slot: NativeCompletionSlot): readonly string[] {
  if (slot.refType === 'ref/resource' && slot.argumentName === 'engineVersion') return NATIVE_ENUM_SETS.engineVersion;
  if (slot.refType === 'ref/resource' && slot.argumentName === 'topic') return NATIVE_ENUM_SETS.topic;
  if (slot.refType === 'ref/prompt' && slot.refId === 'asset-import' && slot.argumentName === 'sourceFormat') return NATIVE_ENUM_SETS.sourceFormat;
  if (slot.refType === 'ref/prompt' && slot.refId === 'sequence-render' && slot.argumentName === 'outputFormat') return NATIVE_ENUM_SETS.outputFormat;
  return [];
}

function nativeHostPathLike(value: string): boolean {
  if (/^[a-zA-Z]:[\\/]/.test(value)) return true;
  if (value.includes('\\') || value.startsWith('~')) return true;
  const lower = value.toLowerCase();
  for (const root of ['/home', '/users', '/etc', '/var', '/root', '/tmp', '/bin', '/opt', '/usr']) {
    if (lower.startsWith(root) && (lower.length === root.length || !/[a-z0-9]/i.test(lower[root.length]))) {
      return true;
    }
  }
  return false;
}

function nativeHasTraversal(value: string): boolean {
  return value.replace(/\\/g, '/').split('/').includes('..');
}

/** McpClassifyUnsafeCompletion (McpCompletionProvider.cpp:158-174). */
export function nativeClassifyUnsafe(argumentName: string, value: string): NativeCompletionGuidanceCode | undefined {
  const lower = argumentName.toLowerCase();
  if (NATIVE_SECRET_FRAGMENTS.some((f) => lower.includes(f))) return NATIVE_COMPLETION_GUIDANCE.SECRET_FIELD;
  if (NATIVE_DESTRUCTIVE_FRAGMENTS.some((f) => lower.includes(f))) return NATIVE_COMPLETION_GUIDANCE.DESTRUCTIVE_FIELD;
  if (nativeHostPathLike(value) || nativeHasTraversal(value)) return NATIVE_COMPLETION_GUIDANCE.UNBOUNDED_PATH;
  return undefined;
}

// Tier ladder — McpCompletionProvider.cpp:141-155.
const TIER_EXACT_PREFIX = 0;
const TIER_SUBSTRING = 1;
const TIER_SUBSEQUENCE = 2;
const TIER_TYPO = 3;
const TIER_NONE = 99;

function nativeWithinOneEdit(a: string, b: string): boolean {
  if (a === b) return true;
  const la = a.length;
  const lb = b.length;
  if (Math.abs(la - lb) > 1) return false;
  let i = 0;
  let j = 0;
  let edits = 0;
  while (i < la && j < lb) {
    if (a[i] === b[j]) {
      i += 1;
      j += 1;
      continue;
    }
    edits += 1;
    if (edits > 1) return false;
    if (la > lb) i += 1;
    else if (lb > la) j += 1;
    else {
      i += 1;
      j += 1;
    }
  }
  if (i < la || j < lb) edits += 1;
  return edits <= 1;
}

function nativeIsSubsequence(needle: string, haystack: string): boolean {
  let n = 0;
  for (let h = 0; h < haystack.length && n < needle.length; h += 1) {
    if (haystack[h] === needle[n]) n += 1;
  }
  return n === needle.length;
}

function nativeTierFor(value: string, prefix: string): number {
  if (prefix.length === 0 || value.startsWith(prefix)) return TIER_EXACT_PREFIX;
  if (value.includes(prefix)) return TIER_SUBSTRING;
  if (nativeIsSubsequence(prefix, value)) return TIER_SUBSEQUENCE;
  if (nativeWithinOneEdit(prefix, value.slice(0, prefix.length)) || nativeWithinOneEdit(prefix, value.slice(0, prefix.length + 1))) {
    return TIER_TYPO;
  }
  return TIER_NONE;
}

/** McpRankCompletionCandidates (McpCompletionProvider.cpp:176-204). */
export function nativeRankCandidates(
  pool: readonly NativeCompletionCandidate[],
  prefix: string,
): readonly NativeCompletionCandidate[] {
  const lowered = prefix.toLowerCase();
  const scored: { candidate: NativeCompletionCandidate; tier: number }[] = [];
  let hasStrongMatch = false;
  for (const candidate of pool) {
    const tier = nativeTierFor(candidate.value.toLowerCase(), lowered);
    if (tier === TIER_NONE) continue;
    if (tier < TIER_TYPO) hasStrongMatch = true;
    scored.push({ candidate, tier });
  }
  const matched = hasStrongMatch ? scored.filter((entry) => entry.tier < TIER_TYPO) : scored;
  matched.sort((a, b) => {
    if (a.tier !== b.tier) return a.tier - b.tier;
    if (a.candidate.value < b.candidate.value) return -1;
    if (a.candidate.value > b.candidate.value) return 1;
    return 0;
  });
  return matched.map((entry) => entry.candidate);
}

/** McpApplyCompletionBudget (McpCompletionProvider.cpp:206-223). */
export function nativeApplyBudget(ranked: readonly NativeCompletionCandidate[]): NativeCompletionResult {
  const values: string[] = [];
  let bytes = 0;
  let truncated = false;
  for (const candidate of ranked) {
    if (values.length >= NATIVE_MAX_COMPLETION_ITEMS) {
      truncated = true;
      break;
    }
    const size = utf8Len(candidate.value);
    if (values.length > 0 && bytes + size > NATIVE_MAX_COMPLETION_BYTES) {
      truncated = true;
      break;
    }
    values.push(candidate.value);
    bytes += size;
  }
  return { values, total: ranked.length, hasMore: truncated || values.length < ranked.length };
}

const NATIVE_EMPTY_COMPLETION: NativeCompletionResult = Object.freeze({
  values: Object.freeze([]) as readonly string[],
  total: 0,
  hasMore: false,
});

function nativeSafeEmpty(code: NativeCompletionGuidanceCode): NativeCompletionOutcome {
  return { completion: NATIVE_EMPTY_COMPLETION, guidanceCode: code };
}

// McpCompletionPools.cpp — the class-alias project-handle pool (the ACTOR_CLASS_ALIASES keys).
const NATIVE_PROJECT_HANDLE_POOL: readonly NativeCompletionCandidate[] = [
  'Actor', 'BlockingVolume', 'Camera', 'CameraActor', 'Character', 'DirectionalLight',
  'Pawn', 'PlayerStart', 'PointLight', 'RectLight', 'SkeletalMeshActor', 'Spline',
  'SplineActor', 'SpotLight', 'StaticMeshActor', 'TriggerBox', 'TriggerSphere',
].map((value) => ({ value, kind: 'project-handle' as const }));

// McpCompletionPools.cpp — a representative canonical (id, parent) sample. Native
// builds the capability pool as {id (capability), parentTool + '.' + the id after
// its first dot (legacy-id)} per record, each tagged with the canonical id.
const NATIVE_CANONICAL_SAMPLE: readonly { readonly id: string; readonly parent: string }[] = [
  { id: 'asset.list', parent: 'manage_asset' },
  { id: 'asset.import', parent: 'manage_asset' },
  { id: 'asset.exists', parent: 'manage_asset' },
  { id: 'asset.validate', parent: 'manage_asset' },
  { id: 'blueprint.get', parent: 'manage_blueprint' },
  { id: 'control_actor.spawn_actor', parent: 'control_actor' },
];

const NATIVE_CAPABILITY_POOL: readonly NativeCompletionCandidate[] = (() => {
  const out: NativeCompletionCandidate[] = [];
  const seen = new Set<string>();
  const add = (value: string, kind: NativeCompletionCandidate['kind'], capabilityId: string): void => {
    if (value.length > 0 && !seen.has(value)) {
      seen.add(value);
      out.push({ value, kind, capabilityId });
    }
  };
  for (const { id, parent } of NATIVE_CANONICAL_SAMPLE) {
    add(id, 'capability', id);
    const dot = id.indexOf('.');
    if (dot >= 0) add(`${parent}.${id.slice(dot + 1)}`, 'legacy-id', id);
  }
  return out;
})();

// The native default session enables every tool (bLoadAllToolsOnStart), so every
// pooled capability id is enabled unless a caller passes a narrower set.
const NATIVE_ALL_ENABLED: ReadonlySet<string> = new Set(
  NATIVE_CAPABILITY_POOL.map((candidate) => candidate.capabilityId).filter((id): id is string => id !== undefined),
);

/**
 * The native completion primitive as the transport now invokes it
 * (McpNativeTransportPrimitives.cpp completion/complete): McpCompleteFromPool with
 * the real capability pool (McpCapabilityCompletionPool), the class-alias
 * project-handle pool (McpProjectHandleCompletionPool), and the session
 * enabled-capability set (McpEnabledCapabilityIds). Enum pools are built inside the
 * orchestration. The pools default to the modeled native pools; a caller may inject
 * a narrower `enabledCapabilityIds` to exercise capability scoping.
 */
export function nativeComplete(
  refType: string,
  refId: string,
  argumentName: string,
  value: string,
  pools: {
    capabilityPool?: readonly NativeCompletionCandidate[];
    projectHandlePool?: readonly NativeCompletionCandidate[];
    enabledCapabilityIds?: ReadonlySet<string>;
  } = {},
): NativeCompletionOutcome {
  if (utf8Len(value) > NATIVE_MAX_COMPLETION_PREFIX_LENGTH) {
    return nativeSafeEmpty(NATIVE_COMPLETION_GUIDANCE.UNBOUNDED_PREFIX);
  }
  const unsafe = nativeClassifyUnsafe(argumentName, value);
  if (unsafe !== undefined) {
    return nativeSafeEmpty(unsafe);
  }
  const slot = nativeResolveSlot(refType, refId, argumentName);
  if (slot === undefined) {
    return nativeSafeEmpty(NATIVE_COMPLETION_GUIDANCE.UNAVAILABLE);
  }

  let pool: readonly NativeCompletionCandidate[];
  if (slot.kind === 'capability') {
    pool = pools.capabilityPool ?? NATIVE_CAPABILITY_POOL;
  } else if (slot.kind === 'project-handle') {
    pool = pools.projectHandlePool ?? NATIVE_PROJECT_HANDLE_POOL;
  } else {
    pool = nativeEnumValues(slot).map((v) => ({ value: v, kind: 'enum' as const }));
  }

  if (slot.capabilityScoped) {
    const enabled = pools.enabledCapabilityIds ?? NATIVE_ALL_ENABLED;
    pool = pool.filter((c) => c.capabilityId === undefined || enabled.has(c.capabilityId));
  }

  const ranked = nativeRankCandidates(pool, value);
  if (ranked.length === 0) {
    return nativeSafeEmpty(NATIVE_COMPLETION_GUIDANCE.NO_MATCH);
  }
  return { completion: nativeApplyBudget(ranked), guidanceCode: null };
}

// ---------------------------------------------------------------------------
// Grounding guards — assert the oracle constants still match live native source.
// SUPPORTING drift detection only, NOT the parity proof (the parity proof in
// prompts-completions-parity.test.ts is 100% executable behavior).
// ---------------------------------------------------------------------------

const NATIVE_MCP_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP',
);

export function readNativeSource(relative: string): string {
  return readFileSync(resolve(NATIVE_MCP_ROOT, relative), 'utf8');
}

/** Parse the six native prompt ids straight out of McpPromptCatalog.cpp. */
export function parseNativePromptIdsFromSource(): string[] {
  const source = readNativeSource('Primitives/McpPromptCatalog.cpp');
  const block = source.slice(source.indexOf('McpWorkflowPromptIds'));
  const ids = [...block.matchAll(/TEXT\("([a-z-]+)"\)/g)].map((m) => m[1]);
  // The Ids array lists each of the six once, in order.
  return ids.slice(0, 6);
}
