// tests/unit/task-38/resources-harness.ts
// Task 38 lane A - shared, EXECUTABLE capture + normalization for cross-transport
// resource parity. The TS side is captured from real production code two ways:
// (1) the linked in-memory MCP SDK transport (list/templates/read - the literal
// wire observable) and (2) the production ResourceReadRouter (typed error codes,
// which the SDK collapses to -32603). Comparison is on NORMALIZED SEMANTICS
// (uri/name/mime/revision/data-shape/error-code), never on stdio/HTTP framing or
// source text - so a mismatch is a real behavioral divergence.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { ClientCapabilities } from '@modelcontextprotocol/sdk/types.js';
import { vi } from 'vitest';

import { InMemoryRevisionProvider } from '../../../src/server/mcp-primitives/resource-revision.js';
import { ResourceError } from '../../../src/resources/resource-errors.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from '../../../src/resources/capability-resources.js';
import { EditorStateResources, type EditorStateSource } from '../../../src/resources/editor-state-resources.js';
import { KnowledgeResources, type AssetLookupSource } from '../../../src/resources/knowledge-resources.js';
import { ResourceReadRouter } from '../../../src/resources/resource-read-router.js';

// --- Normalized shapes (the canonical comparison surface) ---

export interface NormEntry {
  readonly uri: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}
export interface NormTemplate {
  readonly uriTemplate: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}
export interface NormRead {
  readonly uri: string;
  readonly mimeType: string;
  readonly revision: number;
  readonly dataPresent: boolean;
  readonly dataKeys: readonly string[];
}
export interface Mismatch {
  readonly pointer: string;
  readonly ts: unknown;
  readonly native: unknown;
}

interface RawEntryLike {
  uri?: unknown;
  name?: unknown;
  description?: unknown;
  mimeType?: unknown;
}
interface RawTemplateLike {
  uriTemplate?: unknown;
  name?: unknown;
  description?: unknown;
  mimeType?: unknown;
}
interface RawContentLike {
  uri?: unknown;
  mimeType?: unknown;
  revision?: unknown;
  text?: unknown;
}

const str = (value: unknown): string => (typeof value === 'string' ? value : String(value ?? ''));

// --- Normalizers (transport/framing independent) ---

export function normalizeList(resources: readonly RawEntryLike[]): NormEntry[] {
  return resources
    .map((entry) => ({
      uri: str(entry.uri),
      name: str(entry.name),
      description: str(entry.description),
      mimeType: str(entry.mimeType),
    }))
    .sort((a, b) => a.uri.localeCompare(b.uri));
}

export function normalizeTemplates(templates: readonly RawTemplateLike[]): NormTemplate[] {
  return templates
    .map((entry) => ({
      uriTemplate: str(entry.uriTemplate),
      name: str(entry.name),
      description: str(entry.description),
      mimeType: str(entry.mimeType),
    }))
    .sort((a, b) => a.uriTemplate.localeCompare(b.uriTemplate));
}

/**
 * Reduce a single read `content` (TS SDK OR native) to its semantic shape. The
 * revision is read from the top-level field when present (native) else parsed
 * from the serialized `text` (TS carries it only inside `text`). `dataPresent`
 * and `dataKeys` capture whether a real bounded body was returned (TS) versus a
 * stub with no `data` (native).
 */
export function normalizeRead(content: RawContentLike): NormRead {
  let revision = typeof content.revision === 'number' ? content.revision : Number.NaN;
  let dataPresent = false;
  let dataKeys: string[] = [];
  try {
    const parsed = JSON.parse(str(content.text)) as Record<string, unknown>;
    if (Number.isNaN(revision) && typeof parsed.revision === 'number') {
      revision = parsed.revision;
    }
    if (parsed.data !== null && typeof parsed.data === 'object') {
      dataPresent = true;
      dataKeys = Object.keys(parsed.data as Record<string, unknown>).sort();
    }
  } catch (error: unknown) {
    if (!(error instanceof SyntaxError)) throw error;
    // A non-JSON text leaves dataPresent=false; revision stays as read.
  }
  return { uri: str(content.uri), mimeType: str(content.mimeType), revision, dataPresent, dataKeys };
}

// --- Structural diff producing JSON-pointer mismatches ---

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function diff(ts: unknown, native: unknown, pointer = ''): Mismatch[] {
  if (JSON.stringify(ts) === JSON.stringify(native)) {
    return [];
  }
  if (Array.isArray(ts) && Array.isArray(native)) {
    const out: Mismatch[] = [];
    if (ts.length !== native.length) {
      out.push({ pointer: `${pointer}/length`, ts: ts.length, native: native.length });
    }
    const max = Math.max(ts.length, native.length);
    for (let i = 0; i < max; i += 1) {
      out.push(...diff(ts[i], native[i], `${pointer}/${i}`));
    }
    return out;
  }
  if (isPlainObject(ts) && isPlainObject(native)) {
    const out: Mismatch[] = [];
    const keys = new Set([...Object.keys(ts), ...Object.keys(native)]);
    for (const key of [...keys].sort()) {
      out.push(...diff(ts[key], native[key], `${pointer}/${key}`));
    }
    return out;
  }
  return [{ pointer: pointer === '' ? '/' : pointer, ts, native }];
}

// --- Drift injectors (return a deep clone with exactly one field changed) ---

export function driftTemplateField(
  templates: readonly RawTemplateLike[],
  index: number,
  field: keyof NormTemplate,
  value: string,
): RawTemplateLike[] {
  const clone = JSON.parse(JSON.stringify(templates)) as RawTemplateLike[];
  const target = clone[index];
  if (target !== undefined) {
    target[field] = value;
  }
  return clone;
}

export function withStaleRevision(read: NormRead, revision: number): NormRead {
  return { ...read, revision };
}

// --- Executable TS transport capture (linked in-memory MCP SDK) ---

export interface TsTransportCapture {
  readonly list: readonly RawEntryLike[];
  readonly templates: readonly RawTemplateLike[];
  readonly catalogContent: RawContentLike;
}

const LIVE_REVISIONS = { selection: 1, level: 1, assetRegistry: 1, package: 1 } as const;

export async function captureTsTransport(capabilities: ClientCapabilities): Promise<TsTransportCapture> {
  vi.resetModules();
  vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
  vi.stubEnv('NODE_ENV', 'test');
  const { createServer } = await import('../../../src/server/server-factory.js');
  const built = createServer();
  const client = new Client({ name: 'task-38-parity', version: '1.0.0' }, { capabilities });
  const pair = InMemoryTransport.createLinkedPair();
  await built.server.connect(pair[1]);
  await client.connect(pair[0], { timeout: 15000 });
  try {
    const listed = await client.listResources(undefined, { timeout: 15000 });
    const templated = await client.listResourceTemplates(undefined, { timeout: 15000 });
    const read = await client.readResource({ uri: 'ue://capability/catalog' }, { timeout: 15000 });
    return {
      list: listed.resources as readonly RawEntryLike[],
      templates: templated.resourceTemplates as readonly RawTemplateLike[],
      catalogContent: (read.contents[0] ?? {}) as RawContentLike,
    };
  } finally {
    await pair[0].close();
    built.automationBridge?.stop();
    built.bridge?.dispose();
    built.metricsServer?.close();
    vi.unstubAllEnvs();
  }
}

// --- Executable TS semantic engine capture (ResourceReadRouter typed errors) ---

/** A router whose editor/asset sources report UNAVAILABLE (deterministic, no bridge). */
export function unavailableRouter(): ResourceReadRouter {
  const revisions = new InMemoryRevisionProvider();
  const editor: EditorStateSource = {
    isAvailable: async () => false,
    engineVersion: async () => null,
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'None', path: 'None' }),
    selectedActors: async () => [],
    liveRevisions: async () => LIVE_REVISIONS,
  };
  const lookup: AssetLookupSource = {
    isAvailable: async () => false,
    objectExists: async () => false,
    assetExists: async () => false,
  };
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(), revisions),
    new EditorStateResources(editor, revisions, 'ParityProject'),
    new KnowledgeResources(lookup, revisions),
  );
}

export function liveRevisionRouter(): ResourceReadRouter {
  const revisions = new InMemoryRevisionProvider();
  const editor: EditorStateSource = {
    isAvailable: async () => true,
    engineVersion: async () => '5.7',
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'None', path: 'None' }),
    selectedActors: async () => [],
    liveRevisions: async () => LIVE_REVISIONS,
  };
  const lookup: AssetLookupSource = {
    isAvailable: async () => true,
    objectExists: async () => false,
    assetExists: async () => false,
  };
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(), revisions),
    new EditorStateResources(editor, revisions, 'ParityProject'),
    new KnowledgeResources(lookup, revisions),
  );
}

export interface CapturedError {
  readonly code: string;
  readonly uri: string;
  readonly message: string;
}

/** Read a URI through the router, returning its typed ResourceError code or throwing on success. */
export async function captureRouterError(router: ResourceReadRouter, uri: string): Promise<CapturedError> {
  try {
    await router.read(uri);
  } catch (error) {
    if (error instanceof ResourceError) {
      return { code: error.code, uri: error.uri, message: error.message };
    }
    throw error;
  }
  throw new Error(`Expected a ResourceError for ${uri} but the read succeeded`);
}

/** Read a URI through the router and normalize the resulting content. */
export async function captureRouterRead(router: ResourceReadRouter, uri: string): Promise<NormRead> {
  const result = await router.read(uri);
  return normalizeRead((result.contents[0] ?? {}) as RawContentLike);
}
