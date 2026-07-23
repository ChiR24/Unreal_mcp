// tests/unit/task-38/prompts-completions-baseline.test.ts
//
// Task 38 lane B — PASSING baseline characterization.
//
// This suite is GREEN today. It (1) characterizes the CURRENT TypeScript
// production behavior for prompts/list, prompts/get, and completion/complete;
// (2) characterizes the EXECUTABLE native fixture (prompts-completions-native-
// fixture.ts) over the same scenarios; (3) proves the two AGREE on the faithful-
// port semantics (normalized parity baseline: prompt names/titles/order, enum
// completion, refusals, deterministic ranking + budget, unknown-prompt error);
// (4) grounds the fixture constants against live native C++ source (drift guard,
// NOT the parity proof); and (5) records — without failing — the two known
// cross-transport gaps (native static-prompt surface, native empty completion
// pools). The RED parity assertions live in the sibling parity suite.
//
// No transport, no live editor, no wall clock.

import { describe, expect, it } from 'vitest';

import { getPrompt, listPrompts } from '../../../src/server/mcp-primitives/prompts/prompt-catalog.js';
import { PromptError } from '../../../src/server/mcp-primitives/prompts/prompt-errors.js';
import { WORKFLOW_PROMPT_IDS, type PromptReferenceValidator } from '../../../src/server/mcp-primitives/prompts/prompt-types.js';
import { complete } from '../../../src/server/mcp-primitives/completions/completion-provider.js';
import { createStaticCompletionSource } from '../../../src/server/mcp-primitives/completions/completion-sources.js';
import { rankCandidates, applyBudget } from '../../../src/server/mcp-primitives/completions/completion-ranking.js';
import { classifyUnsafe } from '../../../src/server/mcp-primitives/completions/completion-slots.js';
import {
  COMPLETION_GUIDANCE_CODES,
  MAX_PREFIX_LENGTH,
  type CompletionCandidate,
  type CompletionOutcome,
  type CompletionRequest,
} from '../../../src/server/mcp-primitives/completions/completion-types.js';
import { MINIMAL_PROFILE, SessionCapabilityProfile } from '../../../src/server/mcp-primitives/session-capability-profile.js';
import {
  NATIVE_ENUM_SETS,
  NATIVE_SECRET_FRAGMENTS,
  NATIVE_WORKFLOW_PROMPT_IDS,
  nativeApplyBudget,
  nativeClassifyUnsafe,
  nativeComplete,
  nativePromptsGet,
  nativePromptsList,
  nativeRankCandidates,
  parseNativePromptIdsFromSource,
  readNativeSource,
  type NativeCompletionCandidate,
} from './prompts-completions-native-fixture.js';

const SESSION = 'ses-lane-b';

// A permissive reference validator so getPrompt renders without depending on the
// Task 31 evidence fixtures; the fail-closed reference checks are covered by the
// production catalog unit tests, not re-proven here.
const allowAll: PromptReferenceValidator = {
  capabilityExists: () => true,
  resourceExists: () => true,
};

function profile(enabled: readonly string[] = []): SessionCapabilityProfile {
  return new SessionCapabilityProfile(
    { ...MINIMAL_PROFILE, hasCompletions: true },
    { enabledCapabilityIds: () => new Set(enabled) },
  );
}

const source = createStaticCompletionSource();

function tsComplete(ref: CompletionRequest['ref'], name: string, value: string, enabled: readonly string[] = []): CompletionOutcome {
  return complete({ ref, argument: { name, value } }, SESSION, profile(enabled), source);
}

function normalizeTs(o: CompletionOutcome): { values: string[]; total: number; hasMore: boolean; guidanceCode: string | null } {
  return { values: [...o.completion.values], total: o.completion.total, hasMore: o.completion.hasMore, guidanceCode: o.guidance?.code ?? null };
}

function normalizeNative(o: ReturnType<typeof nativeComplete>): { values: string[]; total: number; hasMore: boolean; guidanceCode: string | null } {
  return { values: [...o.completion.values], total: o.completion.total, hasMore: o.completion.hasMore, guidanceCode: o.guidanceCode };
}

const PROMPT_REF = (name: string): CompletionRequest['ref'] => ({ type: 'ref/prompt', name });

// ---------------------------------------------------------------------------
// 1. Current TypeScript behavior (characterization).
// ---------------------------------------------------------------------------

describe('baseline: current TS prompts/completions behavior', () => {
  it('TS prompts/list: six entries in stable order with full MCP metadata', () => {
    const listed = listPrompts();
    expect(listed.map((p) => p.name)).toEqual([...WORKFLOW_PROMPT_IDS]);
    expect(listed).toHaveLength(6);
    for (const entry of listed) {
      expect(entry.title.length).toBeGreaterThan(0);
      expect(entry.description.length).toBeGreaterThan(0);
      for (const arg of entry.arguments) {
        expect(Object.keys(arg).sort()).toEqual(['description', 'name', 'required']);
      }
    }
    // asset-import carries a description and two declared arguments on the TS wire.
    const assetImport = listed.find((p) => p.name === 'asset-import');
    expect(assetImport?.arguments.map((a) => a.name)).toEqual(['destinationPath', 'sourceFormat']);
  });

  it('TS prompts/get: renders one bounded user message with the canonical sequence', () => {
    const out = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, allowAll);
    expect(out.messages).toHaveLength(1);
    expect(out.messages[0].role).toBe('user');
    expect(out.messages[0].content.type).toBe('text');
    const body = out.messages[0].content.text;
    expect(body).toContain('asset.import');
    expect(body).toContain('"operation": "execute"');
    expect(body).toContain('/Game/Imported/Rock'); // interpolates the validated argument
    expect(Buffer.byteLength(body, 'utf8')).toBeLessThanOrEqual(65536);
  });

  it('TS prompts/get: unknown prompt name throws PROMPT_NOT_FOUND', () => {
    try {
      getPrompt('does-not-exist', {}, allowAll);
      throw new Error('expected throw');
    } catch (e) {
      expect(e).toBeInstanceOf(PromptError);
      expect((e as PromptError).code).toBe('PROMPT_NOT_FOUND');
    }
  });

  it('TS prompts/get: argument-level validation is enforced (secret / unknown / missing)', () => {
    const cases: { args: Record<string, string>; code: string }[] = [
      { args: { destinationPath: '/Game/X', apiKey: 'abc' }, code: 'PROMPT_SECRET_ARGUMENT' },
      { args: { destinationPath: '/Game/X', bogus: 'y' }, code: 'PROMPT_UNKNOWN_ARGUMENT' },
      { args: {}, code: 'PROMPT_MISSING_ARGUMENT' },
    ];
    for (const { args, code } of cases) {
      try {
        getPrompt('asset-import', args, allowAll);
        throw new Error(`expected throw for ${code}`);
      } catch (e) {
        expect((e as PromptError).code).toBe(code);
      }
    }
  });

  it('TS completion: enum slot ranks safe suggestions for a prefix', () => {
    const out = tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', 'f');
    expect([...out.completion.values]).toEqual(['fbx', 'gltf']); // fbx (prefix) then gltf (substring)
    expect(out.completion.total).toBe(2);
    expect(out.completion.hasMore).toBe(false);
    expect(out.guidance).toBeUndefined();
  });

  it('TS completion: refuses secret / destructive / host-path fields, never echoing the value', () => {
    const secret = tsComplete({ type: 'ref/resource', uri: 'ue://capability/{capabilityId}' }, 'apiKey', 'sk-live-1');
    expect(secret.completion.values).toEqual([]);
    expect(secret.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.SECRET_FIELD);
    expect(JSON.stringify(secret.guidance)).not.toContain('sk-live-1');

    const destructive = tsComplete(PROMPT_REF('asset-import'), 'confirmDelete', '');
    expect(destructive.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.DESTRUCTIVE_FIELD);

    const hostPath = tsComplete({ type: 'ref/resource', uri: 'ue://asset/{assetPath}' }, 'assetPath', '/etc/passwd');
    expect(hostPath.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.UNBOUNDED_PATH);
  });

  it('TS completion: unknown (ref,argument) pair yields UNAVAILABLE, an unbounded prefix yields UNBOUNDED_PREFIX', () => {
    const unavailable = tsComplete(PROMPT_REF('inspect-fix'), 'newValue', 'Z=1');
    expect(unavailable.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.UNAVAILABLE);

    const unbounded = tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', 'f'.repeat(MAX_PREFIX_LENGTH + 1));
    expect(unbounded.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.UNBOUNDED_PREFIX);
  });
});

// ---------------------------------------------------------------------------
// 2. Native fixture behavior (characterization).
// ---------------------------------------------------------------------------

describe('baseline: executable native fixture behavior', () => {
  it('native prompts/list forwards full metadata (name, title, description, arguments)', () => {
    const listed = nativePromptsList();
    expect(listed.map((p) => p.name)).toEqual([...NATIVE_WORKFLOW_PROMPT_IDS]);
    for (const entry of listed) {
      expect(Object.keys(entry).sort()).toEqual(['arguments', 'description', 'name', 'title']);
      expect(entry.title.length).toBeGreaterThan(0);
      expect(entry.description.length).toBeGreaterThan(0);
    }
    const assetImport = listed.find((p) => p.name === 'asset-import');
    expect(assetImport?.arguments.map((a) => a.name)).toEqual(['destinationPath', 'sourceFormat']);
  });

  it('native prompts/get renders the canonical body and validates arguments', () => {
    // A secret-named argument is refused before rendering.
    const withSecret = nativePromptsGet('asset-import', { apiKey: 'abc' });
    expect(withSecret.ok).toBe(false);
    expect(withSecret.errorCode).toBe('PROMPT_SECRET_ARGUMENT');
    // A valid argument renders the full canonical multi-step body.
    const rendered = nativePromptsGet('asset-import', { destinationPath: '/Game/Imported/Rock' });
    expect(rendered.ok).toBe(true);
    expect(rendered.messages[0].content.text).toContain('asset.import');
    expect(rendered.messages[0].content.text).toContain('/Game/Imported/Rock');
    const unknown = nativePromptsGet('does-not-exist');
    expect(unknown.ok).toBe(false);
    expect(unknown.errorMessage).toBe('Unknown workflow prompt: does-not-exist');
  });

  it('native completion enum slots resolve; capability/project-handle slots draw ranked candidates', () => {
    const enumOutcome = nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', 'f');
    expect([...enumOutcome.completion.values]).toEqual(['fbx', 'gltf']);
    expect(enumOutcome.guidanceCode).toBeNull();

    const capability = nativeComplete('ref/resource', 'ue://capability/{capabilityId}', 'capabilityId', 'asset.');
    expect(capability.completion.values.length).toBeGreaterThan(0);
    expect(capability.completion.values).toContain('asset.list');
    expect(capability.guidanceCode).toBeNull();

    const handle = nativeComplete('ref/resource', 'ue://asset/{assetPath}', 'assetPath', '');
    expect(handle.completion.values.length).toBe(17);
    expect(handle.completion.values).toContain('PointLight');
    expect(handle.guidanceCode).toBeNull();
  });

  it('native completion refuses secret / destructive / host-path fields identically', () => {
    expect(nativeComplete('ref/resource', 'ue://capability/{capabilityId}', 'apiKey', 'sk-1').guidanceCode).toBe('COMPLETION_SECRET_FIELD');
    expect(nativeComplete('ref/prompt', 'asset-import', 'confirmDelete', '').guidanceCode).toBe('COMPLETION_DESTRUCTIVE_FIELD');
    expect(nativeComplete('ref/resource', 'ue://asset/{assetPath}', 'assetPath', '/etc/passwd').guidanceCode).toBe('COMPLETION_UNBOUNDED_PATH');
  });
});

// ---------------------------------------------------------------------------
// 3. Normalized agreement — the faithful-port semantics match today (GREEN).
// ---------------------------------------------------------------------------

describe('baseline: TS and native fixture agree on ported semantics', () => {
  it('agree on prompt names, titles, and order', () => {
    const ts = listPrompts().map((p) => ({ name: p.name, title: p.title }));
    const nat = nativePromptsList().map((p) => ({ name: p.name, title: p.title }));
    expect(nat).toEqual(ts);
  });

  it('agree on the enum completion for sourceFormat=f (safe argument suggestions)', () => {
    const ts = normalizeTs(tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', 'f'));
    const nat = normalizeNative(nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', 'f'));
    expect(nat).toEqual(ts);
  });

  it('agree on the refusal guidance code for a secret argument', () => {
    // Pure-logic classifier parity (the code both providers assign to apiKey).
    expect(nativeClassifyUnsafe('apiKey', '')).toBe(classifyUnsafe('apiKey', ''));
    expect(classifyUnsafe('apiKey', '')).toBe(COMPLETION_GUIDANCE_CODES.SECRET_FIELD);
  });

  it('agree on deterministic ranking + budget for one shared 250-candidate pool', () => {
    const tsPool: CompletionCandidate[] = Array.from({ length: 250 }, (_v, i) => ({
      value: `asset.gen_${String(i).padStart(4, '0')}`,
      kind: 'capability',
      capabilityId: `asset.gen_${String(i).padStart(4, '0')}`,
    }));
    const natPool: NativeCompletionCandidate[] = tsPool.map((c) => ({ value: c.value, kind: 'capability', capabilityId: c.capabilityId }));
    const tsBudget = applyBudget(rankCandidates(tsPool, 'asset.gen_'));
    const natBudget = nativeApplyBudget(nativeRankCandidates(natPool, 'asset.gen_'));
    expect({ values: [...natBudget.values], total: natBudget.total, hasMore: natBudget.hasMore }).toEqual({
      values: [...tsBudget.values],
      total: tsBudget.total,
      hasMore: tsBudget.hasMore,
    });
    expect(tsBudget.values.length).toBe(100);
    expect(tsBudget.total).toBe(250);
    expect(tsBudget.hasMore).toBe(true);
  });

  it('agree on the unknown-prompt error message', () => {
    let tsMessage = '';
    try {
      getPrompt('does-not-exist', {}, allowAll);
    } catch (e) {
      tsMessage = (e as PromptError).message;
    }
    const nat = nativePromptsGet('does-not-exist');
    expect(nat.errorMessage).toBe(tsMessage);
    expect(nat.errorMessage).toBe('Unknown workflow prompt: does-not-exist');
  });
});

// ---------------------------------------------------------------------------
// 4. Grounding guards — fixture constants still match live native C++ source.
//    Supporting drift detection only; NOT the parity proof.
// ---------------------------------------------------------------------------

describe('baseline: native fixture is grounded in live plugin source', () => {
  it('fixture prompt ids match McpPromptCatalog.cpp', () => {
    expect(parseNativePromptIdsFromSource()).toEqual([...NATIVE_WORKFLOW_PROMPT_IDS]);
  });

  it('native prompts/list delegates to McpBuildPromptListEntries, which emits full metadata', () => {
    const transport = readNativeSource('Transport/McpNativeTransportPrimitives.cpp');
    const listBlock = transport.slice(transport.indexOf('Method == TEXT("prompts/list")'), transport.indexOf('Method == TEXT("prompts/get")'));
    expect(listBlock).toContain('McpBuildPromptListEntries()');
    const render = readNativeSource('Primitives/McpPromptRender.cpp');
    expect(render).toContain('SetStringField(TEXT("description"), Prompt.Description)');
    expect(render).toContain('SetArrayField(TEXT("arguments")');
  });

  it('native completion/complete injects the real capability + project-handle pools and enabled set', () => {
    const src = readNativeSource('Transport/McpNativeTransportPrimitives.cpp');
    expect(src).toContain('McpCompleteFromPool(');
    expect(src).toContain('McpCapabilityCompletionPool()');
    expect(src).toContain('McpProjectHandleCompletionPool()');
    expect(src).toContain('McpEnabledCapabilityIds(');
    expect(src).not.toContain('TArray<FMcpCompletionCandidate>(), TArray<FMcpCompletionCandidate>(), TSet<FString>()');
  });

  it('native enum sets and secret fragments match the provider source', () => {
    const src = readNativeSource('Primitives/McpCompletionProvider.cpp');
    for (const v of NATIVE_ENUM_SETS.sourceFormat) {
      expect(src).toContain(`TEXT("${v}")`);
    }
    for (const fragment of NATIVE_SECRET_FRAGMENTS) {
      expect(src).toContain(`TEXT("${fragment}")`);
    }
  });
});

// ---------------------------------------------------------------------------
// 5. The prompt/completion divergences are CLOSED — native now agrees with TS.
// ---------------------------------------------------------------------------

describe('baseline: prompt/completion divergences are closed', () => {
  it('native prompts/get renders the full canonical sequence, byte-equal to TS', () => {
    const tsBody = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, allowAll).messages[0].content.text;
    const natBody = nativePromptsGet('asset-import', { destinationPath: '/Game/Imported/Rock' }).messages[0].content.text;
    expect(natBody).toContain('asset.import');
    expect(natBody).toContain('/Game/Imported/Rock');
    expect(natBody).toBe(tsBody);
  });

  it('native capability completion returns the same ranked candidates as TS', () => {
    const enabled = ['asset.list'];
    const ts = tsComplete({ type: 'ref/resource', uri: 'ue://capability/{capabilityId}' }, 'capabilityId', 'asset.list', enabled);
    const nat = nativeComplete('ref/resource', 'ue://capability/{capabilityId}', 'capabilityId', 'asset.list', { enabledCapabilityIds: new Set(enabled) });
    expect(nat.completion.values.length).toBeGreaterThan(0);
    expect([...nat.completion.values]).toEqual([...ts.completion.values]);
    expect(nat.guidanceCode).toBeNull();
  });
});
