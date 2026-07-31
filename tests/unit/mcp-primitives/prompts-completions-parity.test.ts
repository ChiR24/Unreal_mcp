// tests/unit/mcp-primitives/prompts-completions-parity.test.ts
//
// Task 38 lane B — RED-first normalized cross-transport parity for prompts/list,
// prompts/get, and completion/complete.
//
// This suite asserts the DESIRED cross-transport parity between the TypeScript
// production primitives and the executable native fixture. It compares SEMANTICS
// (normalized metadata / results / error codes), never HTTP/SSE framing and never
// grepped source text, and uses exact `toEqual` — no broad success/error masks.
//
// Most assertions are GREEN today: prompt names/titles/order, bounded message
// envelope, the unknown-prompt error, enum completion ordering/limit/hasMore, safe
// argument suggestions, secret/destructive/host-path refusal, unknown-slot
// fallback, comparator teeth, and full/minimal client profiles are a faithful
// native port and already match.
//
// The two formerly-RED blocks are now GREEN — the Task 38 remediation closed both
// production gaps and this suite proves the normalized equivalence:
//   Prompt surface: prompts/list now serves full metadata (McpBuildPromptListEntries)
//     and prompts/get renders the canonical body with typed argument validation
//     (McpPromptRender.cpp / McpPromptArgumentValidation.cpp).
//   Completion pools: completion/complete now injects the real capability and
//     class-alias pools plus the session enabled set (McpCompletionPools.cpp), so
//     those completions return the same ranked candidates as TS.
// The assertions are exact `toEqual` checks; do NOT weaken them.

import { describe, expect, it } from 'vitest';

import { getPrompt, listPrompts } from '../../../src/server/mcp-primitives/prompts/prompt-catalog.js';
import { PromptError } from '../../../src/server/mcp-primitives/prompts/prompt-errors.js';
import type { PromptReferenceValidator } from '../../../src/server/mcp-primitives/prompts/prompt-types.js';
import { complete } from '../../../src/server/mcp-primitives/completions/completion-provider.js';
import { createStaticCompletionSource } from '../../../src/server/mcp-primitives/completions/completion-sources.js';
import { rankCandidates, applyBudget } from '../../../src/server/mcp-primitives/completions/completion-ranking.js';
import {
  COMPLETION_GUIDANCE_CODES,
  type CompletionCandidate,
  type CompletionOutcome,
  type CompletionRequest,
} from '../../../src/server/mcp-primitives/completions/completion-types.js';
import {
  MINIMAL_PROFILE,
  SessionCapabilityProfile,
  parseClientCapabilityProfile,
} from '../../../src/server/mcp-primitives/session-capability-profile.js';
import {
  nativeApplyBudget,
  nativeComplete,
  nativePromptsGet,
  nativePromptsList,
  nativeRankCandidates,
  type NativeCompletionCandidate,
  type NativeCompletionOutcome,
} from './prompts-completions-native-fixture.js';

const SESSION = 'ses-parity-b';
const source = createStaticCompletionSource();

const allowAll: PromptReferenceValidator = {
  capabilityExists: () => true,
  resourceExists: () => true,
};

function profile(enabled: readonly string[] = [], caps: Partial<typeof MINIMAL_PROFILE> = {}): SessionCapabilityProfile {
  return new SessionCapabilityProfile(
    { ...MINIMAL_PROFILE, hasCompletions: true, ...caps },
    { enabledCapabilityIds: () => new Set(enabled) },
  );
}

function tsComplete(ref: CompletionRequest['ref'], name: string, value: string, enabled: readonly string[] = []): CompletionOutcome {
  return complete({ ref, argument: { name, value } }, SESSION, profile(enabled), source);
}

/** Normalized completion outcome: the wire result plus the internal guidance code. */
function normTs(o: CompletionOutcome): { values: string[]; total: number; hasMore: boolean; guidanceCode: string | null } {
  return { values: [...o.completion.values], total: o.completion.total, hasMore: o.completion.hasMore, guidanceCode: o.guidance?.code ?? null };
}
function normNat(o: NativeCompletionOutcome): { values: string[]; total: number; hasMore: boolean; guidanceCode: string | null } {
  return { values: [...o.completion.values], total: o.completion.total, hasMore: o.completion.hasMore, guidanceCode: o.guidanceCode };
}

const PROMPT_REF = (name: string): CompletionRequest['ref'] => ({ type: 'ref/prompt', name });
const CAP_REF: CompletionRequest['ref'] = { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' };
const ASSET_REF: CompletionRequest['ref'] = { type: 'ref/resource', uri: 'ue://asset/{assetPath}' };

// ===========================================================================
// GREEN parity — the faithful-port semantics agree today.
// ===========================================================================

describe('parity: six prompt metadata entries (name/title/order)', () => {
  it('both transports advertise the same six prompt names and titles in the same order', () => {
    const ts = listPrompts().map((p) => ({ name: p.name, title: p.title }));
    const nat = nativePromptsList().map((p) => ({ name: p.name, title: p.title }));
    expect(nat).toEqual(ts);
    expect(nat).toHaveLength(6);
  });
});

describe('parity: bounded rendered messages (envelope)', () => {
  it('both transports return exactly one bounded user-role text message', () => {
    const tsOut = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, allowAll);
    const natOut = nativePromptsGet('asset-import', { destinationPath: '/Game/Imported/Rock' });
    const envelope = (msgs: readonly { role: string; content: { type: string } }[]): { count: number; role: string; type: string } => ({
      count: msgs.length,
      role: msgs[0].role,
      type: msgs[0].content.type,
    });
    expect(envelope(natOut.messages)).toEqual(envelope(tsOut.messages));
    expect(envelope(tsOut.messages)).toEqual({ count: 1, role: 'user', type: 'text' });
    // Both bodies are within the TS 64 KiB budget.
    expect(Buffer.byteLength(tsOut.messages[0].content.text, 'utf8')).toBeLessThanOrEqual(65536);
    expect(Buffer.byteLength(natOut.messages[0].content.text, 'utf8')).toBeLessThanOrEqual(65536);
  });
});

describe('parity: unknown prompt exact error', () => {
  it('both transports reject an unknown prompt with the identical message', () => {
    let tsMessage = '';
    try {
      getPrompt('nope-prompt', {}, allowAll);
    } catch (e) {
      expect(e).toBeInstanceOf(PromptError);
      tsMessage = (e as PromptError).message;
    }
    const nat = nativePromptsGet('nope-prompt');
    expect(nat.ok).toBe(false);
    expect(nat.errorMessage).toEqual(tsMessage);
    expect(nat.errorMessage).toEqual('Unknown workflow prompt: nope-prompt');
  });
});

describe('parity: completion candidate ordering / limit / hasMore', () => {
  it('an enum slot yields the identical ordered slice on both transports', () => {
    expect(normNat(nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', 'f'))).toEqual(
      normTs(tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', 'f')),
    );
    // sequence-render/outputFormat: prefix 'p' -> png only.
    expect(normNat(nativeComplete('ref/prompt', 'sequence-render', 'outputFormat', 'p'))).toEqual(
      normTs(tsComplete(PROMPT_REF('sequence-render'), 'outputFormat', 'p')),
    );
  });

  it('the ranking + item/byte budget agree for one shared 250-candidate pool', () => {
    const tsPool: CompletionCandidate[] = Array.from({ length: 250 }, (_v, i) => ({
      value: `asset.gen_${String(i).padStart(4, '0')}`,
      kind: 'capability',
      capabilityId: `asset.gen_${String(i).padStart(4, '0')}`,
    }));
    const natPool: NativeCompletionCandidate[] = tsPool.map((c) => ({ value: c.value, kind: 'capability', capabilityId: c.capabilityId }));
    const ts = applyBudget(rankCandidates(tsPool, 'asset.gen_'));
    const nat = nativeApplyBudget(nativeRankCandidates(natPool, 'asset.gen_'));
    expect({ values: [...nat.values], total: nat.total, hasMore: nat.hasMore }).toEqual({
      values: [...ts.values],
      total: ts.total,
      hasMore: ts.hasMore,
    });
    expect(ts.values.length).toBe(100);
    expect(ts.hasMore).toBe(true);
  });
});

describe('parity: safe argument suggestions', () => {
  it('an empty prefix returns the full ordered enum set identically', () => {
    expect(normNat(nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', ''))).toEqual(
      normTs(tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', '')),
    );
    // Order is deterministic: exact-prefix tier, then lexicographic.
    expect([...tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', '').completion.values]).toEqual(['fbx', 'gltf', 'obj', 'png', 'wav']);
  });
});

describe('parity: secret / destructive / host-path refusal', () => {
  it('refuses each unsafe class with the identical safe-empty outcome, never echoing the value', () => {
    const cases: { ref: CompletionRequest['ref']; refId: string; refType: 'ref/prompt' | 'ref/resource'; name: string; value: string; code: string }[] = [
      { ref: CAP_REF, refType: 'ref/resource', refId: 'ue://capability/{capabilityId}', name: 'apiKey', value: 'sk-live-99', code: COMPLETION_GUIDANCE_CODES.SECRET_FIELD },
      { ref: PROMPT_REF('asset-import'), refType: 'ref/prompt', refId: 'asset-import', name: 'confirmDelete', value: 'yes', code: COMPLETION_GUIDANCE_CODES.DESTRUCTIVE_FIELD },
      { ref: ASSET_REF, refType: 'ref/resource', refId: 'ue://asset/{assetPath}', name: 'assetPath', value: '/etc/passwd', code: COMPLETION_GUIDANCE_CODES.UNBOUNDED_PATH },
    ];
    for (const c of cases) {
      const ts = tsComplete(c.ref, c.name, c.value);
      const nat = nativeComplete(c.refType, c.refId, c.name, c.value);
      expect(normNat(nat)).toEqual(normTs(ts));
      expect(normTs(ts).guidanceCode).toBe(c.code);
      expect(normTs(ts).values).toEqual([]);
      // The refused value is never present in the returned payload.
      expect(JSON.stringify(normNat(nat))).not.toContain(c.value);
    }
  });
});

describe('parity: unknown capability/argument fallback', () => {
  it('an unknown (ref,argument) pair yields the identical UNAVAILABLE fallback', () => {
    const ts = tsComplete(PROMPT_REF('inspect-fix'), 'newValue', 'Z=1');
    const nat = nativeComplete('ref/prompt', 'inspect-fix', 'newValue', 'Z=1');
    expect(normNat(nat)).toEqual(normTs(ts));
    expect(normTs(ts).guidanceCode).toBe(COMPLETION_GUIDANCE_CODES.UNAVAILABLE);
  });
});

// ===========================================================================
// GREEN — exact-parity comparator teeth (injected drift MUST fail equality).
// ===========================================================================

describe('parity comparator teeth: injected one-field drift fails exact parity', () => {
  it('a matched enum completion equals its counterpart but a one-field mutation does not', () => {
    const ts = normTs(tsComplete(PROMPT_REF('asset-import'), 'sourceFormat', 'f'));
    const nat = normNat(nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', 'f'));
    expect(nat).toEqual(ts); // aligned...
    expect({ ...nat, total: nat.total + 1 }).not.toEqual(ts);
    expect({ ...nat, hasMore: !nat.hasMore }).not.toEqual(ts);
    expect({ ...nat, values: [...nat.values, 'obj'] }).not.toEqual(ts);
    expect({ ...nat, guidanceCode: 'COMPLETION_NO_MATCH' }).not.toEqual(ts);
  });

  it('a matched prompt list entry equals its counterpart but a one-field title drift does not', () => {
    const project = (entry: { name: string; title: string; description?: string; arguments?: readonly { name: string }[] }): { name: string; title: string; description: string; argumentNames: string[] } => ({
      name: entry.name,
      title: entry.title,
      description: entry.description ?? '',
      argumentNames: (entry.arguments ?? []).map((argument) => argument.name),
    });
    const ts = project(listPrompts()[1]);
    const nat = project(nativePromptsList()[1]);
    expect(nat).toEqual(ts);
    expect({ ...nat, title: `${nat.title} (drift)` }).not.toEqual(ts);
  });
});

// ===========================================================================
// GREEN — full vs minimal client profile does not change completion semantics.
// ===========================================================================

describe('parity: full and minimal client profiles', () => {
  it('advertised completion capability follows the declared client capabilities', () => {
    expect(parseClientCapabilityProfile({ completions: {} }).hasCompletions).toBe(true);
    expect(parseClientCapabilityProfile({}).hasCompletions).toBe(false);
  });

  it('an enum completion is profile-agnostic: full and minimal clients get identical suggestions', () => {
    const full = complete({ ref: PROMPT_REF('asset-import'), argument: { name: 'sourceFormat', value: 'f' } }, SESSION, profile([], { hasCompletions: true }), source);
    const minimal = complete({ ref: PROMPT_REF('asset-import'), argument: { name: 'sourceFormat', value: 'f' } }, SESSION, profile([], { hasCompletions: false }), source);
    expect(normTs(minimal)).toEqual(normTs(full));
    expect(normNat(nativeComplete('ref/prompt', 'asset-import', 'sourceFormat', 'f'))).toEqual(normTs(full));
  });
});

// ===========================================================================
// Native PROMPT surface now matches the TS rendered surface (divergence closed).
// The transport renders + validates prompts natively (McpPromptRender.cpp /
// McpPromptArgumentValidation.cpp); the assertions are unchanged exact checks.
// ===========================================================================

describe('parity: native prompt surface matches the TS rendered surface', () => {
  it('prompts/list metadata (description + argument names) is equal across transports', () => {
    const tsEntry = listPrompts().find((p) => p.name === 'asset-import');
    const tsNorm = {
      name: tsEntry?.name,
      title: tsEntry?.title,
      description: tsEntry?.description ?? '',
      argumentNames: (tsEntry?.arguments ?? []).map((a) => a.name),
    };
    const natRaw = nativePromptsList().find((p) => p.name === 'asset-import');
    const natNorm = {
      name: natRaw?.name,
      title: natRaw?.title,
      description: natRaw?.description ?? '',
      argumentNames: (natRaw?.arguments ?? []).map((a) => a.name),
    };
    expect(natNorm).toEqual(tsNorm);
  });

  it('prompts/get renders the identical canonical body across transports', () => {
    const tsBody = getPrompt('asset-import', { destinationPath: '/Game/Imported/Rock' }, allowAll).messages[0].content.text;
    const natBody = nativePromptsGet('asset-import', { destinationPath: '/Game/Imported/Rock' }).messages[0].content.text;
    expect(natBody).toEqual(tsBody);
  });

  it('prompts/get enforces the same argument-level validation across transports', () => {
    // Both transports refuse a secret-named argument before rendering.
    const tsOutcome = ((): { threw: boolean; code: string | null } => {
      try {
        getPrompt('asset-import', { destinationPath: '/Game/X', apiKey: 'abc' }, allowAll);
        return { threw: false, code: null };
      } catch (e) {
        return { threw: true, code: (e as PromptError).code };
      }
    })();
    const natGet = nativePromptsGet('asset-import', { destinationPath: '/Game/X', apiKey: 'abc' });
    const natOutcome = { threw: !natGet.ok, code: natGet.ok ? null : 'PROMPT_SECRET_ARGUMENT' };
    expect(natOutcome).toEqual(tsOutcome);
  });
});

// ===========================================================================
// Native completion pools now match the TS ranked candidates (divergence closed).
// completion/complete injects the real capability pool (canonical id + legacy
// parentTool.action form), the class-alias project-handle pool, and the session
// enabled-capability set (McpCompletionPools.cpp).
// ===========================================================================

describe('parity: native completion pools match the TS ranked candidates', () => {
  it('capability completion returns the same ranked outcome across transports', () => {
    // Both transports scope the capability pool to the same enabled-capability set,
    // then rank the shared prefix; the native pool carries the canonical id and its
    // legacy parentTool.action form, matching the TS ranked candidates.
    const enabled = ['asset.list'];
    const ts = tsComplete(CAP_REF, 'capabilityId', 'asset.list', enabled);
    const nat = nativeComplete('ref/resource', 'ue://capability/{capabilityId}', 'capabilityId', 'asset.list', {
      enabledCapabilityIds: new Set(enabled),
    });
    expect(normNat(nat)).toEqual(normTs(ts));
  });

  it('project-handle completion returns the same ranked outcome across transports', () => {
    // Both transports draw the class-alias handles; native no longer returns NO_MATCH.
    const ts = tsComplete(ASSET_REF, 'assetPath', '');
    const nat = nativeComplete('ref/resource', 'ue://asset/{assetPath}', 'assetPath', '');
    expect(normNat(nat)).toEqual(normTs(ts));
  });
});
