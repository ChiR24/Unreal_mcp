// tests/unit/mcp-primitives/resources-baseline.test.ts
// Task 38 lane A - BASELINE CHARACTERIZATION. Pins the CURRENT observable output
// of each transport INDEPENDENTLY (no cross-comparison): the executed TS MCP SDK
// resource surface, and the native `/mcp` fixture oracle. It documents ground
// truth and MUST PASS unchanged - it is the stable reference the RED parity gate
// (resources-parity.test.ts) is measured against. Expected values are an
// independent hard-coded oracle, not re-derived from the modules under test.

import { describe, expect, it, vi } from 'vitest';

import {
  captureTsTransport,
  captureRouterError,
  captureRouterRead,
  normalizeList,
  normalizeRead,
  normalizeTemplates,
  unavailableRouter,
  type NormEntry,
} from './resources-harness.js';
import {
  NATIVE_LIST, NATIVE_TEMPLATES, NATIVE_UNSERVED_URIS, nativeRead, isNativeError,
} from './resources-native-fixture.js';

// Each captureTsTransport boots a real MCP SDK server; under the full parallel unit
// suite that can exceed the 10s default. Raise the ceiling only (assertions unchanged).
vi.setConfig({ testTimeout: 30000, hookTimeout: 30000 });

const FULL_CAPS = { experimental: { resources: { subscribe: true } }, elicitation: {} } as const;
const MINIMAL_CAPS = {} as const;

// Independent oracle: the exact public resource surface the TS transport serves today.
const TS_LIST_URIS = [
  'ue://actors',
  'ue://assets',
  'ue://automation-bridge',
  'ue://capability/catalog',
  'ue://editor',
  'ue://health',
  'ue://level',
  'ue://project',
  'ue://selection',
  'ue://state/revisions',
  'ue://version',
] as const;

const TEMPLATE_URIS = [
  'ue://asset/{assetPath}',
  'ue://capability/{capabilityId}',
  'ue://knowledge/{engineVersion}/{topic}',
  'ue://object/{objectPath}',
] as const;

const CATALOG_DATA_KEYS = ['capabilities', 'count', 'totalCount', 'truncated'] as const;
const PROJECT_DATA_KEYS = ['connected', 'contentRoot', 'engineVersion', 'projectName'] as const;
const LIVE_REVISION_DATA_KEYS = ['assetRegistry', 'level', 'package', 'selection'] as const;

const uris = (entries: readonly NormEntry[]): string[] => entries.map((entry) => entry.uri);

describe('Task 38 baseline - TS transport observable (executed via MCP SDK)', () => {
  it('resources/list returns the eleven current resources with full {uri,name,description,mimeType}', async () => {
    const capture = await captureTsTransport(FULL_CAPS);
    const list = normalizeList(capture.list);
    expect(uris(list)).toEqual([...TS_LIST_URIS]);
    // Spot-pin one legacy and one Task-31 entry verbatim (exact MIME/name fields).
    expect(list.find((entry) => entry.uri === 'ue://assets')).toEqual({
      uri: 'ue://assets',
      name: 'Assets',
      description: 'Project assets',
      mimeType: 'application/json',
    });
    expect(list.find((entry) => entry.uri === 'ue://capability/catalog')).toEqual({
      uri: 'ue://capability/catalog',
      name: 'Capability Catalog',
      description: 'Bounded catalog of gateway capabilities with a monotonic revision',
      mimeType: 'application/json',
    });
  });

  it('resources/templates/list returns the four current templates', async () => {
    const capture = await captureTsTransport(FULL_CAPS);
    const templates = normalizeTemplates(capture.templates);
    expect(templates.map((entry) => entry.uriTemplate)).toEqual([...TEMPLATE_URIS]);
    expect(templates.every((entry) => entry.mimeType === 'application/json')).toBe(true);
  });

  it('resources/read ue://capability/catalog returns bounded revisioned data (revision inside text, no top-level revision)', async () => {
    const capture = await captureTsTransport(FULL_CAPS);
    const read = normalizeRead(capture.catalogContent);
    expect(read.uri).toBe('ue://capability/catalog');
    expect(read.mimeType).toBe('application/json');
    expect(read.revision).toBe(1);
    expect(read.dataPresent).toBe(true);
    expect(read.dataKeys).toEqual([...CATALOG_DATA_KEYS]);
    // The SDK content object itself carries NO top-level revision (TS puts it in text).
    expect(capture.catalogContent.revision).toBeUndefined();
  });

  it('a minimal-capability client observes the identical resource list (profile independent)', async () => {
    const full = normalizeList((await captureTsTransport(FULL_CAPS)).list);
    const minimal = normalizeList((await captureTsTransport(MINIMAL_CAPS)).list);
    expect(uris(minimal)).toEqual(uris(full));
  });

  it('resources/read errors carry typed codes at the engine boundary: unknown=NOT_FOUND, unavailable=UNAVAILABLE', async () => {
    const router = unavailableRouter();
    const unknown = await captureRouterError(router, 'ue://nope');
    const unavailable = await captureRouterError(router, 'ue://editor');
    expect(unknown).toEqual({ code: 'RESOURCE_NOT_FOUND', uri: 'ue://nope', message: 'Unknown resource: ue://nope' });
    expect(unavailable.code).toBe('RESOURCE_UNAVAILABLE');
    expect(unavailable.uri).toBe('ue://editor');
    // A known capability record still reads with a visible revision.
    const record = await captureRouterRead(router, 'ue://capability/manage_asset');
    expect(record.revision).toBe(1);
    expect(record.dataPresent).toBe(true);
  });
});

describe('Task 38 baseline - native `/mcp` fixture oracle (independent stand-in)', () => {
  // The old assertion here was "native lists the same eleven as TS". That was
  // TRUE while native could only READ four of them, so it passed throughout the
  // defect it was supposed to catch. Advertising is only worth asserting
  // together with servability, so both halves are asserted below.
  it('native advertises no resource the TS transport does not', () => {
    const nativeUris = normalizeList(NATIVE_LIST.resources).map((entry) => entry.uri);
    const tsUris = new Set<string>(TS_LIST_URIS);
    expect(nativeUris.filter((uri) => !tsUris.has(uri))).toEqual([]);
  });

  it('every uri native ADVERTISES is one native can READ, and the difference from TS is exactly the unserved set', () => {
    const nativeUris = normalizeList(NATIVE_LIST.resources).map((entry) => entry.uri);
    const unreadable = nativeUris.filter((uri) => isNativeError(nativeRead(uri)));
    expect(unreadable).toEqual([]);

    const missing = TS_LIST_URIS.filter((uri) => !nativeUris.includes(uri));
    expect(missing.slice().sort()).toEqual([...NATIVE_UNSERVED_URIS].slice().sort());

    for (const uri of NATIVE_UNSERVED_URIS) {
      const refusal = nativeRead(uri);
      expect(isNativeError(refusal)).toBe(true);
      if (isNativeError(refusal)) {
        expect(refusal.code).toBe('RESOURCE_UNAVAILABLE');
      }
    }
  });

  it('native resources/templates/list mirrors the four TS templates', () => {
    expect(normalizeTemplates(NATIVE_TEMPLATES.resourceTemplates).map((entry) => entry.uriTemplate)).toEqual([
      ...TEMPLATE_URIS,
    ]);
  });

  it('native resources/read of ue://capability/catalog returns bounded revisioned data', () => {
    const result = nativeRead('ue://capability/catalog');
    expect(isNativeError(result)).toBe(false);
    if (isNativeError(result)) {
      return;
    }
    expect(result.contents[0]?.revision).toBe(1); // native keeps revision at content top level
    const read = normalizeRead(result.contents[0] ?? { uri: '', mimeType: '', text: '' });
    expect(read.revision).toBe(1);
    expect(read.dataPresent).toBe(true);
    expect(read.dataKeys).toEqual([...CATALOG_DATA_KEYS]);
  });

  it('native resources/read of ue://project returns bounded revisioned data', () => {
    const result = nativeRead('ue://project');
    expect(isNativeError(result)).toBe(false);
    if (isNativeError(result)) {
      return;
    }
    const read = normalizeRead(result.contents[0] ?? { uri: '', mimeType: '', text: '' });
    expect(read.revision).toBe(1);
    expect(read.dataPresent).toBe(true);
    expect(read.dataKeys).toEqual([...PROJECT_DATA_KEYS]);
  });

  it('native resources/read of ue://state/revisions returns all four counters', () => {
    const result = nativeRead('ue://state/revisions');
    expect(isNativeError(result)).toBe(false);
    if (isNativeError(result)) {
      return;
    }
    const read = normalizeRead(result.contents[0] ?? { uri: '', mimeType: '', text: '' });
    expect(read.revision).toBe(1);
    expect(read.dataKeys).toEqual([...LIVE_REVISION_DATA_KEYS]);
  });

  it('native resources/read: editor-state uris are RESOURCE_UNAVAILABLE, an unknown uri is RESOURCE_NOT_FOUND', () => {
    for (const uri of ['ue://editor', 'ue://selection']) {
      const result = nativeRead(uri);
      expect(isNativeError(result)).toBe(true);
      if (isNativeError(result)) {
        expect(result.code).toBe('RESOURCE_UNAVAILABLE');
        expect(result.jsonRpcCode).toBe(-32600);
      }
    }
    const unknown = nativeRead('ue://nope');
    expect(isNativeError(unknown)).toBe(true);
    if (isNativeError(unknown)) {
      expect(unknown.code).toBe('RESOURCE_NOT_FOUND');
      expect(unknown.jsonRpcCode).toBe(-32602);
    }
  });
});
