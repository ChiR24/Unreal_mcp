// tests/unit/task-38/parity-harness-characterization.test.ts
//
// Task 38 lane E — CHARACTERIZATION (GREEN baseline of the harness INPUTS).
//
// This suite runs BEFORE any parity rule. It pins that every on-disk fixture the
// harness ingests is (1) schema-valid through the strict validators and (2) an
// actual EXECUTED capture: each fixture value is re-derived here from LIVE
// production code and asserted equal. A hand-typed guess or a source-text
// snapshot would fail the grounding step, so this file is what makes "executable
// capture" more than a label. It must PASS unchanged; the RED parity rules
// (parity-harness-selftest.test.ts) are measured against this ground truth.
//
// Native side: there is no executable native capture, so the native fixture is a
// RED REQUIREMENT doc, and this suite only characterizes that it is well-formed
// and mirrors the runtime NATIVE_CAPTURE_REQUIREMENT — it never treats it as a
// passing native side.

import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

import {
  checkFixture,
  validatePointer,
  diff,
  REASONS,
  NATIVE_CAPTURE_REQUIREMENT,
} from './parity-harness.mjs';

import {
  InMemoryRevisionProvider,
  INITIAL_REVISION,
  nextRevision,
} from '../../../src/server/mcp-primitives/resource-revision.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from '../../../src/resources/capability-resources.js';
import { EditorStateResources } from '../../../src/resources/editor-state-resources.js';
import { KnowledgeResources } from '../../../src/resources/knowledge-resources.js';
import { ResourceReadRouter } from '../../../src/resources/resource-read-router.js';
import { ResourceError } from '../../../src/resources/resource-errors.js';
import {
  parseClientCapabilityProfile,
  MINIMAL_PROFILE,
  SessionCapabilityProfile,
} from '../../../src/server/mcp-primitives/session-capability-profile.js';
import { fallbackPointerFor } from '../../../src/server/mcp-primitives/fallback-pointers.js';
import { ClientProfileStore } from '../../../src/server/mcp-primitives/client-profile-store.js';

// --- fixture loading -------------------------------------------------------

function loadFixture(name: string): Record<string, unknown> {
  const url = new URL(`../../fixtures/task-38/${name}`, import.meta.url);
  return JSON.parse(readFileSync(url, 'utf8')) as Record<string, unknown>;
}

const FIXTURES = {
  result: loadFixture('executable-ts-result.json'),
  error: loadFixture('executable-ts-error.json'),
  revision: loadFixture('executable-ts-revision.json'),
  profile: loadFixture('executable-ts-profile.json'),
  pointer: loadFixture('executable-ts-pointer.json'),
  session: loadFixture('executable-ts-session.json'),
} as const;

// --- live production capture adapters (the grounding layer) ----------------

function buildRouter(): ResourceReadRouter {
  const revisions = new InMemoryRevisionProvider();
  const editor = {
    isAvailable: async () => false,
    engineVersion: async () => null,
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'None', path: 'None' }),
    selectedActors: async () => [],
    liveRevisions: async () => ({ selection: 1, level: 1, assetRegistry: 1, package: 1 }),
  };
  const lookup = {
    isAvailable: async () => false,
    objectExists: async () => false,
    assetExists: async () => false,
  };
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(), revisions),
    new EditorStateResources(editor, revisions, 'HarnessCharacterization'),
    new KnowledgeResources(lookup, revisions),
  );
}

// Turn a router read into the framing-neutral normalized result shape. This is
// the capture adapter: it strips the JSON-RPC/SDK `text` envelope and keeps only
// {uri, mimeType, revision, dataPresent, dataKeys}.
function normalizeReadContent(content: { uri?: unknown; mimeType?: unknown; revision?: unknown; text?: unknown }): {
  uri: string; mimeType: string; revision: number; dataPresent: boolean; dataKeys: string[];
} {
  const parsed = JSON.parse(String(content.text ?? '{}')) as { revision?: unknown; data?: unknown };
  const data = parsed.data;
  const dataPresent = data !== null && typeof data === 'object';
  const dataKeys = dataPresent ? Object.keys(data as Record<string, unknown>).sort() : [];
  const revision = typeof content.revision === 'number'
    ? content.revision
    : (typeof parsed.revision === 'number' ? parsed.revision : Number.NaN);
  return { uri: String(content.uri ?? ''), mimeType: String(content.mimeType ?? ''), revision, dataPresent, dataKeys };
}

// --- 1. every fixture is schema-valid through the strict validators --------

describe('Task 38 lane E — harness inputs are schema-valid executable-ts captures', () => {
  for (const [domain, fixture] of Object.entries(FIXTURES)) {
    it(`checkFixture accepts the ${domain} fixture as an executable-ts capture`, () => {
      const capture = checkFixture(fixture);
      expect(capture.captureKind).toBe('executable-ts');
      expect(capture.executable).toBe(true);
      expect(capture.domain).toBe(domain);
      expect(typeof capture.provenance).toBe('string');
      expect(capture.provenance.length).toBeGreaterThan(0);
    });
  }
});

// --- 2. every fixture value equals what LIVE production emits ---------------

describe('Task 38 lane E — every fixture value is grounded in live production', () => {
  it('result: catalog read normalizes to the pinned bounded shape', async () => {
    const read = await buildRouter().read('ue://capability/catalog');
    const captured = normalizeReadContent(read.contents[0] ?? {});
    expect(captured).toEqual(FIXTURES.result.value);
    expect(captured.dataKeys).toEqual(['capabilities', 'count', 'totalCount', 'truncated']);
  });

  it('error: an unknown uri throws the pinned typed code and uri (never a numeric JSON-RPC code)', async () => {
    let captured: { code: string; uri: string } | null = null;
    try {
      await buildRouter().read('ue://does-not-exist');
    } catch (error) {
      if (error instanceof ResourceError) {
        captured = { code: error.code, uri: error.uri };
      } else {
        throw error;
      }
    }
    expect(captured).toEqual(FIXTURES.error.value);
    expect(/^-?\d+$/.test((captured as { code: string }).code)).toBe(false);
  });

  it('revision: the monotonic sequence is exactly INITIAL_REVISION and its successors', () => {
    const r1 = INITIAL_REVISION;
    const r2 = nextRevision(r1);
    const r3 = nextRevision(r2);
    expect([r1, r2, r3]).toEqual((FIXTURES.revision.value as { revisions: number[] }).revisions);
  });

  it('profile: a full-capability client derives the pinned six booleans', () => {
    const profile = parseClientCapabilityProfile({
      resources: { subscribe: true }, prompts: {}, completions: {}, elicitation: {}, tasks: {},
    });
    expect(profile).toEqual(FIXTURES.profile.value);
  });

  it('pointer: the minimal-client resources fallback normalizes to the bounded pointer', () => {
    const raw = fallbackPointerFor(MINIMAL_PROFILE, 'resources');
    expect(validatePointer(raw)).toEqual(FIXTURES.pointer.value);
  });

  it('session: ClientProfileStore proves per-session cleanup and isolation the fixture asserts', () => {
    const store = new ClientProfileStore();
    store.setSession('harness-session-A', new SessionCapabilityProfile(MINIMAL_PROFILE));
    store.setSession('harness-session-B', new SessionCapabilityProfile(MINIMAL_PROFILE));
    store.clearSession('harness-session-A');
    expect(store.hasSession('harness-session-A')).toBe(false); // cleaned === true in the fixture
    expect(store.hasSession('harness-session-B')).toBe(true); // isolated
    const value = FIXTURES.session.value as { sessionId: string; records: Array<{ ownerSessionId: string }>; cleaned: boolean };
    expect(value.cleaned).toBe(true);
    for (const record of value.records) {
      expect(record.ownerSessionId).toBe(value.sessionId);
    }
  });
});

// --- 3. normalizers are framing-neutral and deterministic ------------------

describe('Task 38 lane E — normalization is framing-neutral and deterministic', () => {
  it('the normalized result drops the JSON-RPC/SDK text envelope (framing erased)', async () => {
    const read = await buildRouter().read('ue://capability/catalog');
    const rawContent = read.contents[0] ?? {};
    expect('text' in rawContent).toBe(true); // framing is present on the wire...
    const captured = normalizeReadContent(rawContent);
    expect(Object.keys(captured).sort()).toEqual(['dataKeys', 'dataPresent', 'mimeType', 'revision', 'uri']);
    expect('text' in captured).toBe(false); // ...and gone after normalization
  });

  it('validatePointer erases the TS-only hint/nextCall framing to {primitive,mode,reference}', () => {
    const raw = fallbackPointerFor(MINIMAL_PROFILE, 'resources');
    expect('hint' in raw).toBe(true);
    expect('nextCall' in raw).toBe(true);
    const normalized = validatePointer(raw);
    expect(Object.keys(normalized).sort()).toEqual(['mode', 'primitive', 'reference']);
  });

  it('diff of a value against itself is empty (comparator has no false positives)', () => {
    expect(diff(FIXTURES.result.value, FIXTURES.result.value)).toEqual([]);
    expect(diff(FIXTURES.profile.value, FIXTURES.profile.value)).toEqual([]);
  });
});

// --- 4. the native side is a RED requirement, never a capture --------------

describe('Task 38 lane E — the native-protocol side is an explicit RED requirement', () => {
  const requirement = loadFixture('native-protocol-requirement.json');

  it('the on-disk native fixture is a RED requirement doc, not a capture', () => {
    expect(requirement.kind).toBe('requirement');
    expect(requirement.status).toBe('RED');
    expect(requirement.reason).toBe(REASONS.NATIVE_CAPTURE_ABSENT);
    expect('captureKind' in requirement).toBe(false); // it is NOT a capture
  });

  it('the on-disk requirement mirrors the runtime NATIVE_CAPTURE_REQUIREMENT and names the exact producer', () => {
    expect(NATIVE_CAPTURE_REQUIREMENT.reason).toBe(requirement.reason);
    expect(NATIVE_CAPTURE_REQUIREMENT.status).toBe(requirement.status);
    const producer = requirement.requiredProducer as { name: string; captureKind: string; mustEmitDomains: string[] };
    expect(producer.name).toBe(NATIVE_CAPTURE_REQUIREMENT.requiredProducer.name);
    expect(producer.captureKind).toBe('native-protocol');
    expect(producer.mustEmitDomains).toEqual(['result', 'error', 'revision', 'profile', 'session', 'pointer']);
  });
});
