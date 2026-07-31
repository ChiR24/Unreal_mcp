// Task 38 lane D — shared native-decision model + fixtures for the profiles /
// elicitation / capability-honesty / bounded-fallback parity suite.
//
// This module is NOT a Vitest test (vitest globs only *.test.ts). It provides:
//   1. An INDEPENDENT reimplementation of the native decision rules, driven by
//      the transcribed native oracle (profiles-fallback-native-oracle.json).
//      Comparing the TypeScript runtime output against this model is normalized
//      SEMANTIC parity (decision outputs), never source-text equivalence.
//   2. The normalized-pointer projection that erases transport framing (the TS
//      `hint`/`nextCall` object vs the native single `Reference` string) so the
//      two surfaces are compared on {primitive, mode, reference} only.
//   3. The adversarial capability matrix shared by the characterization and
//      parity suites and the manual-QA driver.

import { readFileSync } from 'node:fs';
import type { ClientCapabilityProfile } from '../../../src/server/mcp-primitives/session-capability-profile.js';
import type { FallbackPointer, FallbackPrimitive } from '../../../src/server/mcp-primitives/fallback-pointers.js';

export interface NativeOracle {
  readonly profileRules: Readonly<Record<string, unknown>>;
  readonly fallback: {
    readonly primitives: readonly FallbackPrimitive[];
    readonly serverBackedPrimitives: readonly FallbackPrimitive[];
    readonly nativeMethod: Readonly<Partial<Record<FallbackPrimitive, string>>>;
    readonly gatewayOperation: Readonly<Record<FallbackPrimitive, string>>;
  };
  readonly serverBackedMethods: readonly string[];
  readonly advertisedSessionCapabilities: Readonly<Record<string, unknown>>;
  readonly errorCodes: { readonly unsupportedMethod: number; readonly refusedButSupported: number };
  readonly directCallMigration: Readonly<Record<string, unknown>>;
  readonly store: Readonly<Record<string, unknown>>;
  readonly elicitation: unknown;
}

export function loadNativeOracle(): NativeOracle {
  const url = new URL('./profiles-fallback-native-oracle.json', import.meta.url);
  return JSON.parse(readFileSync(url, 'utf8')) as NativeOracle;
}

// ---- Independent model of the native structural profile derivation ---------
// Mirrors McpParseSessionCapabilityProfile (McpSessionCapabilityProfile.h). A
// key is "present" only as bare `true` or a JSON object (including `{}`); any
// other shape is absent. Reads only the declared capability object, never a
// client name/version.

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function present(value: unknown): boolean {
  return value === true || isPlainObject(value);
}

function structural(caps: Record<string, unknown>, key: string): boolean {
  if (present(caps[key])) return true;
  const experimental = caps.experimental;
  return isPlainObject(experimental) && present(experimental[key]);
}

const ALL_FALSE: ClientCapabilityProfile = {
  hasResources: false,
  hasPrompts: false,
  hasCompletions: false,
  hasSubscriptions: false,
  hasElicitation: false,
  hasTasks: false,
};

export function nativeProfileFromOracle(capabilities: unknown): ClientCapabilityProfile {
  if (!isPlainObject(capabilities)) return { ...ALL_FALSE };
  const resources = capabilities.resources;
  const subscribeNested = isPlainObject(resources) && resources.subscribe === true;
  return {
    hasResources: structural(capabilities, 'resources'),
    hasPrompts: structural(capabilities, 'prompts'),
    hasCompletions: structural(capabilities, 'completions'),
    hasSubscriptions: subscribeNested || structural(capabilities, 'subscriptions'),
    hasElicitation: structural(capabilities, 'elicitation'),
    hasTasks: structural(capabilities, 'tasks'),
  };
}

// ---- Normalized fallback pointer (framing erased) --------------------------

export interface NormalizedPointer {
  readonly primitive: FallbackPrimitive;
  readonly mode: 'native' | 'gateway';
  readonly reference: string;
}

const SUPPORT_FLAG: Record<FallbackPrimitive, keyof ClientCapabilityProfile> = {
  resources: 'hasResources',
  prompts: 'hasPrompts',
  completions: 'hasCompletions',
  subscriptions: 'hasSubscriptions',
  tasks: 'hasTasks',
};

export function nativeFallbackFromOracle(
  oracle: NativeOracle,
  profile: ClientCapabilityProfile,
  primitive: FallbackPrimitive,
): NormalizedPointer {
  // Native only when the client declares the primitive AND the oracle lists it as
  // server-backed; tasks is client-declarable but unbacked, so it stays gateway.
  const serverBacked = oracle.fallback.serverBackedPrimitives.includes(primitive);
  const nativeMethod = oracle.fallback.nativeMethod[primitive];
  return profile[SUPPORT_FLAG[primitive]] && serverBacked && nativeMethod !== undefined
    ? { primitive, mode: 'native', reference: nativeMethod }
    : { primitive, mode: 'gateway', reference: oracle.fallback.gatewayOperation[primitive] };
}

// Erase the TS-only framing (`hint`, and the `nextCall` object wrapper) so the
// TS pointer is comparable to the native single-string `Reference`.
export function normalizeTsPointer(pointer: FallbackPointer): NormalizedPointer {
  const call = pointer.nextCall as { method?: unknown; operation?: unknown };
  const reference = typeof call.method === 'string' ? call.method
    : typeof call.operation === 'string' ? call.operation
    : '';
  return { primitive: pointer.primitive, mode: pointer.mode, reference };
}

// Native model of McpBuildDirectCallMigration (McpNativeGatewayDirectCallMigration.cpp);
// returns the normalized receipt without `suggestions` (both surfaces compute those identically).

export interface MigrationReceiptCore {
  readonly success: false;
  readonly operation: 'search' | 'describe' | 'execute';
  readonly errorCode: string;
  readonly tool: string;
  readonly message: string;
  readonly nextCall: Record<string, unknown>;
}

const CONTROL_FIELDS = ['action', 'subAction', 'params', 'operation'] as const;

function trimmedString(value: unknown): string | undefined {
  if (typeof value !== 'string') return undefined;
  const trimmed = value.trim();
  return trimmed.length > 0 ? trimmed : undefined;
}

function migratedParamsModel(args: Record<string, unknown>): Record<string, unknown> {
  const nested = isPlainObject(args.params) ? args.params : {};
  const merged: Record<string, unknown> = { ...nested, ...args };
  for (const field of CONTROL_FIELDS) delete merged[field];
  return merged;
}

export function nativeDirectCallFromOracle(
  oracle: NativeOracle,
  toolName: string,
  args: Record<string, unknown>,
  parents: readonly string[],
): MigrationReceiptCore {
  const dc = oracle.directCallMigration as {
    errorCode: string;
    unknownTool: { messageFormat: string };
    knownNoAction: { messageFormat: string };
    knownWithAction: { messageFormat: string };
  };
  const base = { success: false as const, errorCode: dc.errorCode, tool: toolName };
  if (!parents.includes(toolName)) {
    return { ...base, operation: 'search', message: dc.unknownTool.messageFormat.replace('{tool}', toolName), nextCall: { operation: 'search' } };
  }
  const action = trimmedString(args.action) ?? trimmedString(args.subAction);
  if (action === undefined) {
    return { ...base, operation: 'describe', message: dc.knownNoAction.messageFormat.replace('{tool}', toolName), nextCall: { operation: 'describe', tool: toolName } };
  }
  return {
    ...base,
    operation: 'execute',
    message: dc.knownWithAction.messageFormat.replace('{tool}', toolName).replace('{action}', action),
    nextCall: { operation: 'execute', tool: toolName, action, params: migratedParamsModel(args) },
  };
}

// ---- Shared adversarial capability matrix ----------------------------------

export interface CapabilityCase {
  readonly name: string;
  readonly capabilities: unknown;
  readonly note: string;
}

export const CAPABILITY_MATRIX: readonly CapabilityCase[] = [
  { name: 'full', capabilities: { resources: { subscribe: true }, prompts: {}, completions: {}, elicitation: {}, tasks: {} }, note: 'every capability declared as object' },
  { name: 'full-bare-true', capabilities: { resources: true, prompts: true, completions: true, subscriptions: true, elicitation: true, tasks: true }, note: 'every capability as bare boolean true' },
  { name: 'minimal', capabilities: {}, note: 'empty object declares nothing' },
  { name: 'subscribe-nested', capabilities: { resources: { subscribe: true } }, note: 'subscriptions derived from resources.subscribe' },
  { name: 'resources-no-subscribe', capabilities: { resources: {} }, note: 'resources present but no subscribe' },
  { name: 'experimental-nested', capabilities: { experimental: { tasks: {}, completions: true } }, note: 'capabilities under experimental' },
  { name: 'experimental-subscribe-ignored', capabilities: { experimental: { resources: { subscribe: true } } }, note: 'nested experimental resources.subscribe must NOT set subscriptions' },
  { name: 'partial-resources-tasks', capabilities: { resources: {}, tasks: {} }, note: 'partial client lacking prompts/completions/subscriptions' },
  { name: 'false-flags', capabilities: { resources: false, prompts: false, elicitation: false }, note: 'explicit false is absent' },
  { name: 'string-true-injection', capabilities: { resources: 'true', subscriptions: 'yes', tasks: 'enabled' }, note: 'string values must never enable a capability' },
  { name: 'number-injection', capabilities: { resources: 1, prompts: 0, completions: 42 }, note: 'numbers must never enable a capability' },
  { name: 'array-injection', capabilities: { resources: ['x'], tasks: [] }, note: 'arrays must never enable a capability' },
  { name: 'brand-only', capabilities: { name: 'Definitely-Trusted-Client', title: 'Cursor', version: '99.0' }, note: 'brand fields must derive nothing' },
  { name: 'brand-with-caps-a', capabilities: { elicitation: {}, name: 'evil-tool' }, note: 'same caps, brand A' },
  { name: 'brand-with-caps-b', capabilities: { elicitation: {}, name: 'trusted-tool' }, note: 'same caps, brand B — must equal brand A' },
  { name: 'malformed-null', capabilities: null, note: 'null yields all-false' },
  { name: 'malformed-array', capabilities: [], note: 'array yields all-false' },
  { name: 'malformed-string', capabilities: 'resources', note: 'string yields all-false' },
  { name: 'malformed-number', capabilities: 42, note: 'number yields all-false' },
  { name: 'malformed-bool', capabilities: true, note: 'bare true yields all-false' },
];
