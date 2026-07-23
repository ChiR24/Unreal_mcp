// tests/unit/task-38/parity-harness-native-capture.test.ts
//
// Task 38 lane E — OFFLINE proof of the native-protocol INGESTION side. It proves
// the harness can turn the raw artifact the compiled C++ automation test emits into
// admissible native-protocol captures AND that every anti-fabrication tooth bites:
//   * transcript provenance is a closed schema (captureKind "native-protocol" only);
//   * a modelled ("native-model") or source ("source-text") side is refused;
//   * ground-truth requires each value to re-derive from a recorded transcript entry;
//   * the fs verifier rejects a stale/future/tampered/source-drifted capture.
//
// The end-to-end block assembles a FRESH capture from the offline mirror and runs the
// real parity diff — proving the gate is not vacuous and that it correctly reports the
// honest native/TS divergences (native omits the data body; native uses a different
// resource error taxonomy). This is a SIMULATION to exercise the ingestion; it is NOT
// project evidence of parity. The real captures exist only after the integration lane
// runs the C++ test against a built editor, so cross-transport parity stays RED.

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { afterAll, describe, expect, it } from 'vitest';

import {
  checkFixture,
  compareCaptures,
  assertParityReady,
  isGenuineNativeCapture,
  REASONS,
  NATIVE_CAPTURE_REQUIREMENT,
  NATIVE_MECHANISMS,
  NATIVE_PROTOCOL_VERSIONS,
  NATIVE_TRANSCRIPT_KEYS,
  validateNativeTranscript,
  normalizeTranscriptEntry,
  verifyGroundTruth,
  buildNativeCaptureBundle,
  verifyNativeCaptureProvenance,
  computePluginSourceHash,
  computePluginPackageHash,
  serializeTranscriptJsonl,
  sha256Hex,
  SOURCE_HASH_FILES,
  VERIFIER_REASONS,
} from './parity-harness.mjs';

const REPO_ROOT = fileURLToPath(new URL('../../../', import.meta.url));

function loadFixture(rel: string): Record<string, unknown> {
  return JSON.parse(readFileSync(new URL(`../../fixtures/task-38/${rel}`, import.meta.url), 'utf8')) as Record<string, unknown>;
}

function reasonOf(thunk: () => unknown): string {
  try {
    thunk();
  } catch (error) {
    const reason = (error as { reason?: string }).reason;
    if (typeof reason === 'string') return reason;
    throw new Error(`threw a non-HarnessRejection: ${String(error)}`);
  }
  throw new Error('expected a HarnessRejection but nothing was thrown');
}

const ACCEPT_POINTER = loadFixture('native-capture/accept-pointer.json');
const GOOD_TRANSCRIPT = ACCEPT_POINTER.transcript as Record<string, unknown>;
const EMITTED_SAMPLE = loadFixture('native-capture/emitted-sample.json');
const TS_POINTER = loadFixture('executable-ts-pointer.json');

/** A FRESH copy of the offline emitted mirror with capturedAt set to `whenIso`. */
function freshEmitted(whenIso: string): Record<string, unknown> {
  return { ...structuredClone(EMITTED_SAMPLE), capturedAt: whenIso };
}

const tempDirs: string[] = [];
function ownedTempDir(): string {
  const dir = mkdtempSync('/tmp/opencode/task38-nativecap-');
  tempDirs.push(dir);
  return dir;
}
afterAll(() => {
  for (const dir of tempDirs) {
    try {
      rmSync(dir, { recursive: true, force: true });
    } catch {
      /* best effort cleanup of owned scratch */
    }
  }
});

// ===========================================================================
// 1. The transcript provenance block is a closed, strict schema.
// ===========================================================================

describe('Task 38 lane E — native transcript provenance is a closed strict schema', () => {
  it('accepts a well-formed transcript and normalizes all nine fields', () => {
    const parsed = validateNativeTranscript(GOOD_TRANSCRIPT);
    expect(Object.keys(parsed).sort()).toEqual([...NATIVE_TRANSCRIPT_KEYS].sort());
    expect(NATIVE_MECHANISMS).toContain(parsed.mechanism);
    expect(NATIVE_PROTOCOL_VERSIONS).toContain(parsed.protocolVersion);
  });

  it('rejects an unknown transcript field (closed object)', () => {
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, extra: 'x' }))).toBe(REASONS.UNKNOWN_FIELD);
  });

  it('rejects a non-executed mechanism (a model oracle can never masquerade)', () => {
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, mechanism: 'native-model-oracle' }))).toBe(REASONS.MALFORMED);
  });

  it('rejects a legacy or fictional protocol version (native surface is strict)', () => {
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, protocolVersion: '2024-11-05' }))).toBe(REASONS.MALFORMED);
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, protocolVersion: '2026-07-28' }))).toBe(REASONS.MALFORMED);
  });

  it('rejects a non-hex digest and a non-ISO capturedAt and an unbounded transcriptRef', () => {
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, transcriptSha256: 'not-a-sha' }))).toBe(REASONS.MALFORMED);
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, capturedAt: 'yesterday' }))).toBe(REASONS.MALFORMED);
    expect(reasonOf(() => validateNativeTranscript({ ...GOOD_TRANSCRIPT, transcriptRef: '../escape.jsonl' }))).toBe(REASONS.MALFORMED);
  });
});

// ===========================================================================
// 2. captureKind gating: only native-protocol carries a transcript.
// ===========================================================================

describe('Task 38 lane E — only a native-protocol capture may carry a transcript', () => {
  it('accepts the golden native-protocol capture with its transcript', () => {
    const capture = checkFixture(ACCEPT_POINTER);
    expect(capture.captureKind).toBe('native-protocol');
    expect(capture.executable).toBe(true);
    expect(capture.transcript).toBeTruthy();
  });

  const manifest = loadFixture('native-capture/_expected.json').expected as Record<string, string>;
  for (const [file, expectedReason] of Object.entries(manifest)) {
    it(`native-capture/${file} is rejected with ${expectedReason}`, () => {
      expect(reasonOf(() => checkFixture(loadFixture(`native-capture/${file}`)))).toBe(expectedReason);
    });
  }

  it('rejects an executable-ts capture that smuggles a transcript block (UNKNOWN_FIELD)', () => {
    const smuggled = { ...TS_POINTER, transcript: GOOD_TRANSCRIPT };
    expect(reasonOf(() => checkFixture(smuggled))).toBe(REASONS.UNKNOWN_FIELD);
  });
});

// ===========================================================================
// 3. A modelled or absent native side is refused (never a fake PASS).
// ===========================================================================

describe('Task 38 lane E — modelled/absent native sides are refused', () => {
  const ts = checkFixture(TS_POINTER);

  it('a native-model capture is schema-valid but NOT genuine, and parity refuses it', () => {
    const model = checkFixture(loadFixture('native-capture/model-not-genuine.json'));
    expect(model.executable).toBe(false);
    expect(isGenuineNativeCapture(model)).toBe(false);
    const gate = assertParityReady(ts, model);
    expect(gate.ready).toBe(false);
    expect(gate.ready === false && gate.blocker).toBe(NATIVE_CAPTURE_REQUIREMENT);
  });

  it('a genuine transcript-backed native-protocol capture opens the gate', () => {
    const native = checkFixture(ACCEPT_POINTER);
    expect(isGenuineNativeCapture(native)).toBe(true);
    expect(assertParityReady(ts, native).ready).toBe(true);
  });

  it('an absent native side is blocked with the exact RED requirement', () => {
    expect(assertParityReady(ts, null).ready).toBe(false);
  });
});

// ===========================================================================
// 4. Ground-truth: every capture re-derives from a recorded transcript entry.
// ===========================================================================

describe('Task 38 lane E — ground-truth ties every capture to its transcript', () => {
  it('the offline mirror passes ground-truth for all six domains', () => {
    const result = verifyGroundTruth(EMITTED_SAMPLE);
    expect(result.ok).toBe(true);
    expect(result.captureCount).toBe(6);
    expect(result.mismatches).toEqual([]);
  });

  it('the native result normalizes to a data-less body (the honest native divergence)', () => {
    const readEntry = (EMITTED_SAMPLE.transcript as Array<Record<string, unknown>>)[0];
    const norm = normalizeTranscriptEntry(readEntry);
    expect(norm?.domain).toBe('result');
    expect(norm?.value).toEqual({ uri: 'ue://capability/catalog', mimeType: 'application/json', revision: 1, dataPresent: false, dataKeys: [] });
  });

  it('a tampered capture value fails ground-truth (fabrication is caught)', () => {
    const tampered = structuredClone(EMITTED_SAMPLE) as Record<string, unknown>;
    const captures = tampered.captures as Array<{ domain: string; value: { revisions?: number[] } }>;
    const revision = captures.find((c) => c.domain === 'revision');
    if (revision) revision.value.revisions = [9, 9, 9];
    const result = verifyGroundTruth(tampered);
    expect(result.ok).toBe(false);
    expect(result.mismatches.map((m) => m.domain)).toContain('revision');
  });

  it('an evidence-only transcript entry (prompts/list) is not a capture source', () => {
    const promptEntry = (EMITTED_SAMPLE.transcript as Array<Record<string, unknown>>).find((e) => e.method === 'prompts/list');
    expect(normalizeTranscriptEntry(promptEntry)).toBeNull();
  });
});

// ===========================================================================
// 5. The fs provenance verifier rejects stale/future/tampered/drifted captures.
// ===========================================================================

describe('Task 38 lane E — the fs provenance verifier bites', () => {
  function stage(whenIso: string): { dir: string; capture: ReturnType<typeof checkFixture>; sourceHash: string; packageHash: string } {
    const dir = ownedTempDir();
    const bundle = buildNativeCaptureBundle(freshEmitted(whenIso), { root: REPO_ROOT });
    writeFileSync(join(dir, bundle.transcriptRef), bundle.transcriptJsonl, 'utf8');
    const pointer = bundle.captures.find((c) => (c as { domain: string }).domain === 'pointer') as Record<string, unknown>;
    return { dir, capture: checkFixture(pointer), sourceHash: bundle.sourceHash, packageHash: bundle.packageHash };
  }

  it('a fresh capture with a matching transcript + source + package verifies', () => {
    const { dir, capture, sourceHash, packageHash } = stage(new Date().toISOString());
    const result = verifyNativeCaptureProvenance(capture, { captureRoot: dir, expectedSourceHash: sourceHash, expectedPackageHash: packageHash });
    expect(result.ok).toBe(true);
  });

  it('a stale capture is rejected (CAPTURE_STALE)', () => {
    const { dir, capture } = stage(new Date(Date.now() - 3 * 24 * 60 * 60 * 1000).toISOString());
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir }).reason).toBe(VERIFIER_REASONS.CAPTURE_STALE);
  });

  it('a future-dated capture is rejected (CAPTURE_FUTURE)', () => {
    const { dir, capture } = stage(new Date(Date.now() + 60 * 60 * 1000).toISOString());
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir }).reason).toBe(VERIFIER_REASONS.CAPTURE_FUTURE);
  });

  it('a missing transcript file is rejected (TRANSCRIPT_MISSING)', () => {
    const { dir, capture } = stage(new Date().toISOString());
    rmSync(join(dir, 'native-transcript.jsonl'));
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir }).reason).toBe(VERIFIER_REASONS.TRANSCRIPT_MISSING);
  });

  it('a tampered transcript is rejected (TRANSCRIPT_SHA_MISMATCH)', () => {
    const { dir, capture } = stage(new Date().toISOString());
    writeFileSync(join(dir, 'native-transcript.jsonl'), 'tampered\n', 'utf8');
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir }).reason).toBe(VERIFIER_REASONS.TRANSCRIPT_SHA_MISMATCH);
  });

  it('a source-drifted capture is rejected (SOURCE_DRIFT)', () => {
    const { dir, capture } = stage(new Date().toISOString());
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir, expectedSourceHash: 'f'.repeat(64) }).reason).toBe(VERIFIER_REASONS.SOURCE_DRIFT);
  });

  it('a package-drifted capture is rejected (PACKAGE_DRIFT)', () => {
    const { dir, capture } = stage(new Date().toISOString());
    expect(verifyNativeCaptureProvenance(capture, { captureRoot: dir, expectedPackageHash: 'e'.repeat(64) }).reason).toBe(VERIFIER_REASONS.PACKAGE_DRIFT);
  });

  it('a non-native capture is rejected (NOT_NATIVE)', () => {
    const ts = checkFixture(TS_POINTER);
    expect(verifyNativeCaptureProvenance(ts, { captureRoot: '/tmp/opencode' }).reason).toBe(VERIFIER_REASONS.NOT_NATIVE);
  });
});

// ===========================================================================
// 6. Source and package hashes are deterministic anchors.
// ===========================================================================

describe('Task 38 lane E — source/package hashes anchor the capture to the build', () => {
  it('the source hash is a deterministic 64-hex over the plugin sources', () => {
    const a = computePluginSourceHash(REPO_ROOT);
    const b = computePluginSourceHash(REPO_ROOT);
    expect(a).toMatch(/^[0-9a-f]{64}$/);
    expect(a).toBe(b);
  });

  it('the source hash covers the C++ producer and differs from the package hash', () => {
    expect(SOURCE_HASH_FILES).toContain('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/Task38NativeProtocolCaptureTests.cpp');
    expect(computePluginSourceHash(REPO_ROOT)).not.toBe(computePluginPackageHash(REPO_ROOT));
  });

  it('serializeTranscriptJsonl is one JSON per line and shas stably', () => {
    const jsonl = serializeTranscriptJsonl(EMITTED_SAMPLE);
    expect(jsonl.trimEnd().split('\n')).toHaveLength(13);
    expect(sha256Hex(jsonl)).toBe(sha256Hex(serializeTranscriptJsonl(EMITTED_SAMPLE)));
  });
});

// ===========================================================================
// 7. End-to-end: a fresh native capture reaches parity AND surfaces divergence.
// ===========================================================================

describe('Task 38 lane E — end-to-end assemble → verify → parity (SIMULATED, not project evidence)', () => {
  const dir = ownedTempDir();
  const bundle = buildNativeCaptureBundle(freshEmitted(new Date().toISOString()), { root: REPO_ROOT });
  writeFileSync(join(dir, bundle.transcriptRef), bundle.transcriptJsonl, 'utf8');
  const sourceHash = computePluginSourceHash(REPO_ROOT);
  const packageHash = computePluginPackageHash(REPO_ROOT);
  const tsFixtures: Record<string, string> = {
    result: 'executable-ts-result.json', error: 'executable-ts-error.json', revision: 'executable-ts-revision.json',
    profile: 'executable-ts-profile.json', session: 'executable-ts-session.json', pointer: 'executable-ts-pointer.json',
  };

  it('assembles six schema-valid, provenance-verified, parity-ready native captures', () => {
    expect(bundle.captures).toHaveLength(6);
    for (const raw of bundle.captures) {
      const domain = (raw as { domain: string }).domain;
      const native = checkFixture(raw as Record<string, unknown>);
      expect(native.captureKind).toBe('native-protocol');
      expect(verifyNativeCaptureProvenance(native, { captureRoot: dir, expectedSourceHash: sourceHash, expectedPackageHash: packageHash }).ok).toBe(true);
      const ts = checkFixture(loadFixture(tsFixtures[domain]));
      expect(assertParityReady(ts, native).ready).toBe(true);
    }
  });

  it('matches TS on revision/profile/pointer/session and surfaces the honest divergences on result/error', () => {
    const byDomain = new Map(bundle.captures.map((c) => [(c as { domain: string }).domain, checkFixture(c as Record<string, unknown>)]));
    for (const domain of ['revision', 'profile', 'pointer', 'session']) {
      const ts = checkFixture(loadFixture(tsFixtures[domain]));
      const cmp = compareCaptures(ts, byDomain.get(domain));
      expect(cmp.ready === true && cmp.mismatches).toEqual([]);
    }
    const resultCmp = compareCaptures(checkFixture(loadFixture('executable-ts-result.json')), byDomain.get('result'));
    const resultPointers = resultCmp.ready === true ? resultCmp.mismatches.map((m) => m.pointer) : [];
    expect(resultCmp.ready === true && resultCmp.drift).toBe(true);
    expect(resultPointers).toContain('/dataPresent'); // native omits the data body
    expect(resultPointers).toContain('/dataKeys/length');
    const errorCmp = compareCaptures(checkFixture(loadFixture('executable-ts-error.json')), byDomain.get('error'));
    expect(errorCmp.ready === true && errorCmp.mismatches.map((m) => m.pointer)).toEqual(['/code']); // native uses a different resource error taxonomy
  });
});

// ===========================================================================
// 8. The requirement mirrors the real producer and stays RED.
// ===========================================================================

describe('Task 38 lane E — the requirement names the executable producer and stays RED', () => {
  const producer = NATIVE_CAPTURE_REQUIREMENT.requiredProducer;

  it('names the C++ automation test, the runner, and an executed mechanism', () => {
    expect(NATIVE_CAPTURE_REQUIREMENT.status).toBe('RED');
    expect(producer.captureKind).toBe('native-protocol');
    expect(NATIVE_MECHANISMS).toContain(producer.mechanism);
    expect(producer.source).toContain('Task38NativeProtocolCaptureTests.cpp');
    expect(producer.runner).toBe('scripts/qa/task-38-native-capture.mjs');
    expect(NATIVE_PROTOCOL_VERSIONS).toContain(producer.provenance.protocolVersion);
  });
});
