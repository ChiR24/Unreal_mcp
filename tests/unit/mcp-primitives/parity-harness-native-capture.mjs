// @ts-check
// tests/unit/mcp-primitives/parity-harness-native-capture.mjs
// Task 38 lane E — the NATIVE-PROTOCOL INGESTION side of the harness. The schema
// half (the closed transcript-provenance block, mechanism/protocol/hash shapes)
// lives in parity-harness-validators.mjs; THIS module is the runtime half that
// turns the raw artifact emitted by the compiled C++ automation test
// (Private/Tests/NativeProtocolCaptureTests.cpp) into admissible
// native-protocol captures and proves they are genuine, fresh, and not fabricated.
//
// Three teeth:
//   1. ground-truth  — every capture value must re-derive from a transcript entry
//      the test actually recorded (normalizeTranscriptEntry); a value with no
//      matching entry is a fabrication and fails verifyGroundTruth.
//   2. transcript sha — the assembled capture pins the sha-256 of the exact
//      transcript bytes; verifyNativeCaptureProvenance re-hashes the on-disk
//      transcript and rejects any drift.
//   3. freshness/source/package — the capture carries capturedAt + sourceHash +
//      packageHash; the verifier rejects a stale capture, a future timestamp, or a
//      capture whose source/package hash no longer matches the current plugin.
//
// Pure ESM so the Vitest offline tests and the plain-node runner
// (the native capture seam, NativeProtocolCaptureTests.cpp) share exactly one implementation.

import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { isPlainObject } from './parity-harness-schema.mjs';

/** The plugin sources whose bytes DEFINE the captured native behavior. A change to
 * any of them changes sourceHash, so a capture from a stale build is detectable. */
export const SOURCE_HASH_FILES = Object.freeze([
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/NativeProtocolCaptureTests.cpp',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Resources/McpResourceCatalog.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Resources/McpResourceUri.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpResourceRevision.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpSessionCapabilityProfile.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpSubscriptionStore.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpPromptCatalog.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpCompletionProvider.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/DynamicTools/McpSessionConfigureStore.h',
]);

/** The build-identity anchor: the plugin manifest + module rules. */
export const PACKAGE_HASH_FILES = Object.freeze([
  'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/McpAutomationBridge.Build.cs',
]);

/** The verifier's closed failure taxonomy (separate from the schema rejection reasons). */
export const VERIFIER_REASONS = Object.freeze({
  NOT_NATIVE: 'NOT_NATIVE',
  TRANSCRIPT_MISSING: 'TRANSCRIPT_MISSING',
  TRANSCRIPT_SHA_MISMATCH: 'TRANSCRIPT_SHA_MISMATCH',
  CAPTURE_STALE: 'CAPTURE_STALE',
  CAPTURE_FUTURE: 'CAPTURE_FUTURE',
  SOURCE_DRIFT: 'SOURCE_DRIFT',
  PACKAGE_DRIFT: 'PACKAGE_DRIFT',
});

export const DEFAULT_MAX_AGE_MS = 24 * 60 * 60 * 1000;
const PROFILE_FIELDS = ['hasResources', 'hasPrompts', 'hasCompletions', 'hasSubscriptions', 'hasElicitation', 'hasTasks'];

/** @param {string} text @returns {string} */
export function sha256Hex(text) {
  return createHash('sha256').update(text, 'utf8').digest('hex');
}

/** @param {unknown} v @returns {Record<string, unknown>} */
function obj(v) {
  return isPlainObject(v) ? /** @type {Record<string, unknown>} */ (v) : {};
}

/** Order-insensitive canonical string of any JSON value, for deep-equality. @param {unknown} x @returns {string} */
function canonical(x) {
  if (Array.isArray(x)) return `[${x.map(canonical).join(',')}]`;
  if (isPlainObject(x)) {
    const o = /** @type {Record<string, unknown>} */ (x);
    return `{${Object.keys(o).sort().map((k) => `${JSON.stringify(k)}:${canonical(o[k])}`).join(',')}}`;
  }
  return JSON.stringify(x) ?? 'null';
}

/**
 * Re-derive the normalized {domain, value} from ONE raw transcript entry, using the
 * exact framing-neutral rules the C++ test emitted with. Returns null for an entry
 * that is not a capture source (e.g. a prompts/list evidence line).
 * @param {unknown} entry @returns {{ domain: string, value: unknown }|null}
 */
export function normalizeTranscriptEntry(entry) {
  const e = obj(entry);
  const method = typeof e.method === 'string' ? e.method : '';
  const request = obj(e.request);
  const response = obj(e.response);
  if (method === 'resources/read') {
    let parsed = {};
    try {
      parsed = obj(JSON.parse(typeof response.text === 'string' ? response.text : '{}'));
    } catch {
      parsed = {};
    }
    const data = obj(parsed).data;
    const dataPresent = isPlainObject(data);
    const dataKeys = dataPresent ? Object.keys(/** @type {Record<string, unknown>} */ (data)).sort() : [];
    const revision = typeof response.revision === 'number'
      ? response.revision
      : (typeof obj(parsed).revision === 'number' ? obj(parsed).revision : Number.NaN);
    return { domain: 'result', value: { uri: String(response.uri ?? ''), mimeType: String(response.mimeType ?? ''), revision, dataPresent, dataKeys } };
  }
  if (method === 'resources/read-invalid') {
    return { domain: 'error', value: { code: String(response.error ?? ''), uri: String(request.uri ?? '') } };
  }
  if (method === 'configure/mutate') {
    const revisions = Array.isArray(response.revisions) ? response.revisions.map(Number) : [];
    return { domain: 'revision', value: { uri: String(response.uri ?? ''), revisions } };
  }
  if (method === 'session/profile') {
    /** @type {Record<string, boolean>} */
    const value = {};
    for (const field of PROFILE_FIELDS) value[field] = response[field] === true;
    return { domain: 'profile', value };
  }
  if (method === 'fallback/pointer') {
    return { domain: 'pointer', value: { primitive: String(response.primitive ?? ''), mode: String(response.mode ?? ''), reference: String(response.reference ?? '') } };
  }
  if (method === 'session/summary') {
    const records = Array.isArray(response.records)
      ? response.records.map((r) => ({ uri: String(obj(r).uri ?? ''), ownerSessionId: String(obj(r).ownerSessionId ?? '') }))
      : [];
    return { domain: 'session', value: { sessionId: String(response.sessionId ?? ''), records, cleaned: response.cleaned === true } };
  }
  return null;
}

/**
 * Ground-truth gate: every emitted capture value must equal the normalization of one
 * of the transcript entries it cites. A fabricated capture (no matching entry, or a
 * value inconsistent with its entry) is reported — this is what a hand-authored oracle
 * cannot fake, because it would have to also forge a consistent execution transcript.
 * @param {unknown} rawEmitted @returns {{ ok: boolean, mismatches: Array<{ id: string, domain: string }>, captureCount: number }}
 */
export function verifyGroundTruth(rawEmitted) {
  const root = obj(rawEmitted);
  const transcript = Array.isArray(root.transcript) ? root.transcript : [];
  /** @type {Map<number, unknown>} */
  const bySeq = new Map();
  for (const raw of transcript) {
    const entry = obj(raw);
    if (Number.isInteger(entry.seq)) bySeq.set(/** @type {number} */ (entry.seq), raw);
  }
  const captures = Array.isArray(root.captures) ? root.captures : [];
  /** @type {Array<{ id: string, domain: string }>} */
  const mismatches = [];
  for (const rawCap of captures) {
    const cap = obj(rawCap);
    const sourceSeq = Array.isArray(cap.sourceSeq) ? cap.sourceSeq : [];
    const matched = sourceSeq
      .map((s) => normalizeTranscriptEntry(bySeq.get(Number(s))))
      .some((d) => d != null && d.domain === cap.domain && canonical(d.value) === canonical(cap.value));
    if (!matched) mismatches.push({ id: String(cap.id ?? '?'), domain: String(cap.domain ?? '?') });
  }
  return { ok: mismatches.length === 0, mismatches, captureCount: captures.length };
}

/** @param {readonly string[]} files @param {string} root @returns {string} */
function hashFiles(files, root) {
  const h = createHash('sha256');
  for (const rel of files) {
    h.update(rel);
    h.update('\0');
    h.update(readFileSync(join(root, rel)));
    h.update('\0');
  }
  return h.digest('hex');
}

/** @param {string} root @returns {string} */
export function computePluginSourceHash(root) {
  return hashFiles(SOURCE_HASH_FILES, root);
}

/** @param {string} root @returns {string} */
export function computePluginPackageHash(root) {
  return hashFiles(PACKAGE_HASH_FILES, root);
}

/** One transcript entry serialized per line — the exact bytes the sha pins. @param {unknown} rawEmitted @returns {string} */
export function serializeTranscriptJsonl(rawEmitted) {
  const transcript = Array.isArray(obj(rawEmitted).transcript) ? /** @type {unknown[]} */ (obj(rawEmitted).transcript) : [];
  return `${transcript.map((e) => JSON.stringify(e)).join('\n')}\n`;
}

/**
 * Turn the raw C++-emitted artifact into harness-shaped native-protocol captures,
 * stamping each with the transcript/source/package provenance the runner computed.
 * The returned objects are UNVALIDATED — the caller runs checkFixture on each.
 * @param {unknown} rawEmitted
 * @param {{ transcriptRef: string, transcriptSha256: string, sourceHash: string, packageHash: string }} prov
 * @returns {Array<Record<string, unknown>>}
 */
export function assembleNativeCaptures(rawEmitted, prov) {
  const root = obj(rawEmitted);
  const captures = Array.isArray(root.captures) ? root.captures : [];
  /** @type {import('./parity-harness-validators.mjs').NativeTranscript} */
  const transcript = {
    mechanism: String(root.mechanism ?? ''),
    testName: String(root.testName ?? ''),
    engineVersion: String(root.engineVersion ?? ''),
    protocolVersion: String(root.protocolVersion ?? ''),
    capturedAt: String(root.capturedAt ?? ''),
    transcriptRef: prov.transcriptRef,
    transcriptSha256: prov.transcriptSha256,
    sourceHash: prov.sourceHash,
    packageHash: prov.packageHash,
  };
  return captures.map((rawCap) => {
    const cap = obj(rawCap);
    return {
      id: String(cap.id ?? ''),
      captureKind: 'native-protocol',
      domain: String(cap.domain ?? ''),
      match: 'exact',
      provenance: `native automation ${transcript.testName} (${transcript.mechanism}); transcript ${transcript.transcriptRef}`,
      transcript: { ...transcript },
      value: cap.value,
    };
  });
}

/**
 * Build the complete verifiable bundle from one raw artifact, WITHOUT writing files:
 * the transcript bytes + its sha, the current source/package hashes, and the assembled
 * captures. The runner writes the transcript to disk under captureRoot/transcriptRef.
 * @param {unknown} rawEmitted @param {{ root: string, transcriptRef?: string }} opts
 */
export function buildNativeCaptureBundle(rawEmitted, opts) {
  const transcriptRef = opts.transcriptRef ?? 'native-transcript.jsonl';
  const transcriptJsonl = serializeTranscriptJsonl(rawEmitted);
  const transcriptSha256 = sha256Hex(transcriptJsonl);
  const sourceHash = computePluginSourceHash(opts.root);
  const packageHash = computePluginPackageHash(opts.root);
  const captures = assembleNativeCaptures(rawEmitted, { transcriptRef, transcriptSha256, sourceHash, packageHash });
  return { transcriptRef, transcriptJsonl, transcriptSha256, sourceHash, packageHash, captures };
}

/**
 * The fs half of the native gate. Given a schema-validated native-protocol Capture and
 * the directory its transcriptRef is relative to, prove: the transcript exists and
 * re-hashes to the pinned sha; the capture is fresh (not future, not older than
 * maxAgeMs); and, when expected hashes are supplied, its source/package hash still
 * matches the current plugin. Returns a closed result — it never throws for a
 * verification miss, so a runner can report the exact reason.
 * @param {import('./parity-harness-validators.mjs').Capture} capture
 * @param {{ captureRoot: string, now?: number, maxAgeMs?: number, expectedSourceHash?: string, expectedPackageHash?: string }} opts
 * @returns {{ ok: boolean, reason?: string, detail?: string }}
 */
export function verifyNativeCaptureProvenance(capture, opts) {
  const t = capture?.transcript;
  if (capture?.captureKind !== 'native-protocol' || !t) {
    return { ok: false, reason: VERIFIER_REASONS.NOT_NATIVE, detail: 'capture is not a native-protocol capture with a transcript block' };
  }
  const now = opts.now ?? Date.now();
  const maxAgeMs = opts.maxAgeMs ?? DEFAULT_MAX_AGE_MS;
  const transcriptPath = join(opts.captureRoot, t.transcriptRef);
  if (!existsSync(transcriptPath)) {
    return { ok: false, reason: VERIFIER_REASONS.TRANSCRIPT_MISSING, detail: `no transcript at ${t.transcriptRef}` };
  }
  const actualSha = sha256Hex(readFileSync(transcriptPath, 'utf8'));
  if (actualSha !== t.transcriptSha256) {
    return { ok: false, reason: VERIFIER_REASONS.TRANSCRIPT_SHA_MISMATCH, detail: `transcript sha ${actualSha} != pinned ${t.transcriptSha256}` };
  }
  const capturedAt = Date.parse(t.capturedAt);
  if (capturedAt > now) {
    return { ok: false, reason: VERIFIER_REASONS.CAPTURE_FUTURE, detail: `capturedAt ${t.capturedAt} is in the future` };
  }
  if (now - capturedAt > maxAgeMs) {
    return { ok: false, reason: VERIFIER_REASONS.CAPTURE_STALE, detail: `capture is ${Math.round((now - capturedAt) / 1000)}s old, exceeds ${Math.round(maxAgeMs / 1000)}s` };
  }
  if (opts.expectedSourceHash && t.sourceHash !== opts.expectedSourceHash) {
    return { ok: false, reason: VERIFIER_REASONS.SOURCE_DRIFT, detail: `sourceHash ${t.sourceHash} != current ${opts.expectedSourceHash}` };
  }
  if (opts.expectedPackageHash && t.packageHash !== opts.expectedPackageHash) {
    return { ok: false, reason: VERIFIER_REASONS.PACKAGE_DRIFT, detail: `packageHash ${t.packageHash} != current ${opts.expectedPackageHash}` };
  }
  return { ok: true };
}

/** Read + JSON-parse the raw artifact the C++ test wrote (throws if absent/invalid). @param {string} path @returns {unknown} */
export function loadEmittedArtifact(path) {
  return JSON.parse(readFileSync(path, 'utf8'));
}
