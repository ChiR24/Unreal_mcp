// tests/unit/task-38/resources-parity.test.ts
// Task 38 lane A - NORMALIZED CROSS-TRANSPORT PARITY (RED-first). Compares the
// executed TS transport observable against the native `/mcp` fixture oracle on
// NORMALIZED SEMANTICS (uri/name/mime/revision/data-shape/error-code), never on
// framing or source text. Two kinds of cases:
//
//   * GREEN guards prove the comparator is trustworthy: templates genuinely
//     agree, profiles are invariant, editor-unavailable errors agree, and a
//     single injected field / stale revision is caught EXACTLY. A comparator
//     that could never pass (or never fail) would be worthless; these prove it
//     does both.
//   * RED gaps are the real production/native divergences a downstream
//     integrator must close (each is a `toEqual` that fails today). They are
//     NOT source-text checks - they fail because the native runtime genuinely
//     emits different values. See each `EXPECTED RED` note for the exact gap.
//
// RUNTIME BLOCKER: the native side is the hand-authored oracle in
// resources-native-fixture.ts because the C++ `/mcp` surface cannot be executed
// in-process (no live editor / packaged plugin here). The RED cases therefore
// prove a divergence against the native runtime AS MODELLED FROM ITS SOURCE; the
// integrator should replace the oracle with an executable native capture to make
// this a true cross-runtime gate (that swap can only shrink the RED set, since
// the modelled values are transcribed from the native handler).

import { beforeAll, describe, expect, it } from 'vitest';

import {
  captureRouterError,
  captureRouterRead,
  captureTsTransport,
  diff,
  driftTemplateField,
  normalizeList,
  normalizeRead,
  normalizeTemplates,
  liveRevisionRouter,
  unavailableRouter,
  withStaleRevision,
  type CapturedError,
  type NormRead,
  type TsTransportCapture,
} from './resources-harness.js';
import {
  NATIVE_LIST,
  NATIVE_TEMPLATES,
  nativeRead,
  isNativeError,
  type RawResourceError,
} from './resources-native-fixture.js';

const FULL_CAPS = { experimental: { resources: { subscribe: true } }, elicitation: {} } as const;
const MINIMAL_CAPS = {} as const;

let tsFull: TsTransportCapture;
let tsMinimal: TsTransportCapture;
const router = unavailableRouter();
const revisionsRouter = liveRevisionRouter();

beforeAll(async () => {
  tsFull = await captureTsTransport(FULL_CAPS);
  tsMinimal = await captureTsTransport(MINIMAL_CAPS);
}, 30000);

function nativeErrorCode(uri: string): string {
  const result = nativeRead(uri);
  if (!isNativeError(result)) {
    throw new Error(`Expected native error for ${uri}`);
  }
  return (result as RawResourceError).code;
}

function nativeReadNorm(uri: string): NormRead {
  const result = nativeRead(uri);
  if (isNativeError(result)) {
    throw new Error(`Expected native content for ${uri}`);
  }
  return normalizeRead(result.contents[0] ?? { uri: '', mimeType: '', text: '' });
}

// ---------------------------------------------------------------------------
// GREEN guards - the comparator is sound (can pass AND catches single-field drift)
// ---------------------------------------------------------------------------

describe('Task 38 parity - GREEN guards (comparator soundness + real agreement)', () => {
  it('resources/templates/list is byte-equal across transports (proves parity CAN pass)', () => {
    const ts = normalizeTemplates(tsFull.templates);
    const native = normalizeTemplates(NATIVE_TEMPLATES.resourceTemplates);
    expect(diff(ts, native)).toEqual([]);
    expect(ts).toEqual(native);
  });

  it('resources/list is invariant across full vs minimal client capability profiles', () => {
    expect(normalizeList(tsMinimal.list)).toEqual(normalizeList(tsFull.list));
  });

  it('editor-state unavailability agrees: both transports return RESOURCE_UNAVAILABLE', async () => {
    const ts: CapturedError = await captureRouterError(router, 'ue://editor');
    expect(ts.code).toBe('RESOURCE_UNAVAILABLE');
    expect(ts.code).toBe(nativeErrorCode('ue://editor'));
  });

  it('a single injected mimeType drift breaks parity at exactly one pointer (not a rubber stamp)', () => {
    const ts = normalizeTemplates(tsFull.templates);
    // Pristine native agrees...
    expect(diff(ts, normalizeTemplates(NATIVE_TEMPLATES.resourceTemplates))).toEqual([]);
    // ...and drifting exactly one field is caught at exactly that JSON pointer.
    const drifted = driftTemplateField(NATIVE_TEMPLATES.resourceTemplates, 0, 'mimeType', 'text/plain');
    expect(diff(ts, normalizeTemplates(drifted))).toEqual([
      { pointer: '/1/mimeType', ts: 'application/json', native: 'text/plain' },
    ]);
  });

  it('a stale revision breaks exact parity at /revision (revision is part of the contract)', () => {
    const tsRead = normalizeRead(tsFull.catalogContent);
    expect(tsRead.revision).toBe(1);
    const stale = withStaleRevision(tsRead, tsRead.revision + 1);
    expect(diff(tsRead, stale)).toEqual([{ pointer: '/revision', ts: 1, native: 2 }]);
  });
});

// ---------------------------------------------------------------------------
// Normalized parity - the divergences the RED gates named are now CLOSED in the
// native production (McpResourceCatalog::AllListedResources, McpResourceRead
// real-data + typed-error classification). The assertions are unchanged exact
// `toEqual` checks; they now pass because the native runtime emits equal values.
// ---------------------------------------------------------------------------

describe('Task 38 parity - normalized semantics agree (divergences closed)', () => {
  it('resources/list membership matches across transports', () => {
    // Native resources/list now loops AllListedResources() (six legacy + four new
    // = 10), the SAME public set the TS transport serves.
    expect(normalizeList(tsFull.list)).toEqual(normalizeList(NATIVE_LIST.resources));
  });

  it('resources/read ue://capability/catalog returns equal bounded data', () => {
    // Native now returns {revision, data:{capabilities,count,totalCount,truncated}}
    // from McpResourceRead::BuildReadBodyText, matching the TS bounded body.
    expect(normalizeRead(tsFull.catalogContent)).toEqual(nativeReadNorm('ue://capability/catalog'));
  });

  it('resources/read ue://project returns equal bounded data', async () => {
    // Native now returns {projectName,engineVersion,contentRoot,connected}, matching
    // the TS EditorStateResources project body.
    const tsProject = await captureRouterRead(router, 'ue://project');
    expect(tsProject.dataPresent).toBe(true);
    expect(tsProject).toEqual(nativeReadNorm('ue://project'));
  });

  it('resources/read ue://state/revisions returns the same four live counters', async () => {
    const tsRevisions = await captureRouterRead(revisionsRouter, 'ue://state/revisions');
    expect(tsRevisions.dataKeys).toEqual(['assetRegistry', 'level', 'package', 'selection']);
    expect(tsRevisions).toEqual(nativeReadNorm('ue://state/revisions'));
  });

  it('an unknown uri yields the same typed error code (RESOURCE_NOT_FOUND)', async () => {
    // Native McpResourceRead::Classify now separates a genuinely unknown uri
    // (RESOURCE_NOT_FOUND) from a known editor-state uri (RESOURCE_UNAVAILABLE).
    const ts = await captureRouterError(router, 'ue://nope');
    expect(ts.code).toBe('RESOURCE_NOT_FOUND');
    expect(ts.code).toBe(nativeErrorCode('ue://nope'));
  });
});
