// @ts-check
// tests/unit/mcp-primitives/parity-harness-native-requirement.mjs
// Task 38 lane E — the REFUSAL that keeps the harness honest.
//
// Cross-transport parity can only be GREEN when BOTH sides are executed runtime
// captures: an `executable-ts` capture AND a `native-protocol` capture. The
// native-protocol side is now PRODUCIBLE — the compiled C++ automation test
// NativeProtocolCaptureTests.cpp runs the native primitive handlers in a
// live editor and the native capture seam (NativeProtocolCaptureTests.cpp) stamps
// its output with ground-truth + transcript/source/package provenance. But that
// producer has NOT been run against a built editor here (no UBT/editor), so no
// native-protocol capture artifact exists on disk yet. Therefore this module:
//   1. Publishes NATIVE_CAPTURE_REQUIREMENT — the EXACT executable producer +
//      runner the integrator invokes, and the provenance it must carry, and
//   2. Provides assertParityReady(), which REFUSES to declare parity ready unless
//      a genuine native-protocol capture (executed, transcript-backed) is present.
//      A hand-authored oracle (`native-model`) or a source snapshot (`source-text`)
//      is NOT accepted — it yields the RED blocker, never a fake PASS.
//
// There is no code path here that turns a modelled, grepped, or not-yet-run native
// side into a satisfied parity claim.

import { REASONS, isPlainObject } from './parity-harness-schema.mjs';

/** @typedef {import('./parity-harness-validators.mjs').Capture} Capture */

/**
 * The explicit, machine-readable RED requirement. `status: 'RED'` is deliberate:
 * until the integrator RUNS the producer below against a built editor and drops a
 * verified `native-protocol` capture on disk, cross-transport parity is blocked.
 */
export const NATIVE_CAPTURE_REQUIREMENT = Object.freeze({
  reason: REASONS.NATIVE_CAPTURE_ABSENT,
  status: 'RED',
  summary:
    'The executable native-protocol capture seam exists but has not been run against a built UE editor; no native-protocol capture artifact is on disk, so cross-transport parity is BLOCKED-RED and must not be claimed complete.',
  whyBlocked:
    'The producer below is a compiled C++ automation test that executes the native primitive handlers in a live editor. Building and running the editor is deferred to the integration lane (no UBT/editor here), so the capture artifact does not yet exist and assertParityReady stays RED.',
  requiredProducer: Object.freeze({
    name: 'McpAutomationBridge.Task38.NativeProtocolCapture',
    kind: 'cpp-automation-test',
    mechanism: 'native-automation-inprocess',
    source: 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/NativeProtocolCaptureTests.cpp',
    runner: 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests/NativeProtocolCaptureTests.cpp',
    captureKind: 'native-protocol',
    mustEmitDomains: Object.freeze(['result', 'error', 'revision', 'profile', 'session', 'pointer']),
    emits:
      'Runs the compiled McpResourceCatalog / McpResourceUri / McpSessionCapabilityProfile / McpSubscriptionStore / McpSessionConfigureStore / McpPromptCatalog / McpCompletionProvider handlers in-process and writes task38-native-capture.json { transcript[], captures[] } to $MCP_TASK38_CAPTURE_DIR.',
    provenance: Object.freeze({
      transcriptRef: '<relative path to the emitted native-transcript.jsonl>',
      transcriptSha256: '<sha-256 of the transcript bytes each capture re-derives from>',
      sourceHash: '<sha-256 of the plugin sources that produced the behavior>',
      packageHash: '<sha-256 of the built plugin package/manifest>',
      capturedAt: '<ISO-8601 capture time (freshness anchor)>',
      protocolVersion: '2025-11-25',
    }),
    groundTruth:
      'verifyGroundTruth requires each capture value to re-derive from a transcript entry it cites; verifyNativeCaptureProvenance re-hashes the transcript and checks freshness/source/package — a fabricated or stale capture cannot pass.',
  }),
  forbiddenSubstitutes: Object.freeze([
    'A hand-authored native oracle (captureKind "native-model") — admissible for characterization ONLY, never for a completion claim.',
    'A grep/source snapshot (captureKind "source-text") — rejected outright by validateCapture.',
    'A native-protocol capture missing its transcript provenance block, or one whose transcript sha / source hash / freshness no longer verifies.',
  ]),
  unblocksWhen:
    'The integration lane builds the plugin, runs the native capture seam against a serialized UE5.7 editor, and the emitted native-protocol captures pass ground-truth + provenance verification; assertParityReady then opens and compareCaptures runs the exact diff.',
});

/**
 * A native side is genuine ONLY when it is an executed native-protocol capture that
 * carries its transcript provenance block. captureKind alone is insufficient — a
 * native-protocol capture with no transcript is not a validated capture at all.
 * @param {Capture|null|undefined} capture
 * @returns {boolean}
 */
export function isGenuineNativeCapture(capture) {
  return (
    capture != null &&
    capture.executable === true &&
    capture.captureKind === 'native-protocol' &&
    isPlainObject(capture.transcript)
  );
}

/**
 * @typedef {{ ready: true }} ParityReady
 * @typedef {{ ready: false, blocker: typeof NATIVE_CAPTURE_REQUIREMENT, detail: string }} ParityBlocked
 */

/**
 * Gate a parity comparison. Returns `{ ready: true }` ONLY when the TS side is an
 * executable-ts capture AND the native side is a genuine, transcript-backed
 * native-protocol capture. Anything else — an absent native side, a modelled oracle,
 * a source snapshot, or a native-protocol capture with no transcript — returns the RED
 * blocker. There is no argument that fakes readiness.
 * @param {Capture} tsCapture
 * @param {Capture|null|undefined} nativeCapture
 * @returns {ParityReady|ParityBlocked}
 */
export function assertParityReady(tsCapture, nativeCapture) {
  if (!(tsCapture.executable && tsCapture.captureKind === 'executable-ts')) {
    return { ready: false, blocker: NATIVE_CAPTURE_REQUIREMENT, detail: `TS side is not an executable-ts capture (got captureKind "${tsCapture.captureKind}")` };
  }
  if (nativeCapture == null) {
    return { ready: false, blocker: NATIVE_CAPTURE_REQUIREMENT, detail: 'native side is absent' };
  }
  if (!isGenuineNativeCapture(nativeCapture)) {
    return { ready: false, blocker: NATIVE_CAPTURE_REQUIREMENT, detail: `native side is "${nativeCapture.captureKind}" without a verified transcript, not an executed native-protocol capture — a modelled, grepped, or unverified native side cannot satisfy a completion claim` };
  }
  return { ready: true };
}
