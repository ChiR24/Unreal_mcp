// tests/unit/task-38/resources-native-fixture.ts
// Task 38 lane A — the native `/mcp` resource oracle.
//
// RUNTIME BLOCKER (reported, not worked around): the native resource surface is
// C++ (`FMcpNativeTransport::HandlePrimitiveMethod` in
// `plugins/.../MCP/Transport/McpNativeTransportPrimitives.cpp`, delegating to
// `MCP/Resources/McpResourceReadContent.cpp` and `MCP/Resources/McpResourceCatalog.h`).
// It only answers over HTTP/SSE from a live UE editor or a packaged plugin, so it
// CANNOT be executed in-process from Vitest/Node. This file is the SMALLEST
// stand-in fixture in the owned path: an INDEPENDENT oracle that models the
// JSON-RPC RESULTS the native handler now emits — transcribed from that handler's
// runtime behavior, NOT imported from the TypeScript modules under test. A parity
// mismatch against it is therefore a genuine TS/native divergence. When the
// integrator provides an executable native capture, swap this oracle for that
// recording and the parity gate becomes a true cross-runtime gate.
//
// Provenance of every value below (native source):
//   resources/list        -> McpResourceCatalog::AllListedResources() (6 legacy + 4 new = 10)
//   resources/templates   -> McpResourceCatalog::Templates()          (4 defs)
//   resources/read ok      -> McpResourceRead::BuildReadBodyText for the two socket-readable
//                            URIs (ue://capability/catalog, ue://project): a bounded
//                            {"revision":1,"data":{...}} body with real key sets
//   resources/read error  -> McpResourceRead::Classify: a listed static resource or a
//                            template-instance uri that is not socket-readable returns
//                            RESOURCE_UNAVAILABLE (-32600); any other uri returns
//                            RESOURCE_NOT_FOUND (-32602).

export interface RawResourceEntry {
  readonly uri: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}

export interface RawTemplateEntry {
  readonly uriTemplate: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}

export interface RawReadContent {
  readonly uri: string;
  readonly mimeType: string;
  readonly revision?: number;
  readonly text: string;
}

export interface RawReadResult {
  readonly contents: readonly RawReadContent[];
}

export interface RawResourceError {
  readonly code: string;
  readonly uri: string;
  readonly message: string;
  /** JSON-RPC numeric code the native BuildError uses. */
  readonly jsonRpcCode: number;
}

const JSON_MIME = 'application/json';
const NATIVE_INVALID_REQUEST = -32600;
const NATIVE_INVALID_PARAMS = -32602;

/** native resources/list — the six legacy resources followed by the four new ones. */
export const NATIVE_LIST: { readonly resources: readonly RawResourceEntry[] } = {
  resources: [
    { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: JSON_MIME },
    { uri: 'ue://actors', name: 'Actors', description: 'Actors in the current level', mimeType: JSON_MIME },
    { uri: 'ue://level', name: 'Current Level', description: 'Current level name and path', mimeType: JSON_MIME },
    { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: JSON_MIME },
    {
      uri: 'ue://automation-bridge',
      name: 'Automation Bridge',
      description: 'Automation bridge diagnostics and recent activity',
      mimeType: JSON_MIME,
    },
    { uri: 'ue://version', name: 'Engine Version', description: 'Unreal Engine version and compatibility info', mimeType: JSON_MIME },
    {
      uri: 'ue://capability/catalog',
      name: 'Capability Catalog',
      description: 'Bounded catalog of gateway capabilities with a monotonic revision',
      mimeType: JSON_MIME,
    },
    { uri: 'ue://project', name: 'Project', description: 'Redacted project name, engine version, and content root', mimeType: JSON_MIME },
    { uri: 'ue://editor', name: 'Editor State', description: 'Bounded editor state: PIE status and current level', mimeType: JSON_MIME },
    { uri: 'ue://selection', name: 'Selection', description: 'Bounded list of selected actor handles', mimeType: JSON_MIME },
  ],
};

/** native resources/templates/list — the same 4 templates as the TS catalog. */
export const NATIVE_TEMPLATES: { readonly resourceTemplates: readonly RawTemplateEntry[] } = {
  resourceTemplates: [
    {
      uriTemplate: 'ue://capability/{capabilityId}',
      name: 'Capability Record',
      description: 'Bounded record for one capability (identifier, category, action count; no full schema)',
      mimeType: JSON_MIME,
    },
    {
      uriTemplate: 'ue://knowledge/{engineVersion}/{topic}',
      name: 'Engine Knowledge',
      description: 'Stable Unreal knowledge keyed by engine version and topic',
      mimeType: JSON_MIME,
    },
    {
      uriTemplate: 'ue://object/{objectPath}',
      name: 'Object Reference',
      description: 'Normalized handle for an object at a UE content path',
      mimeType: JSON_MIME,
    },
    {
      uriTemplate: 'ue://asset/{assetPath}',
      name: 'Asset Reference',
      description: 'Normalized handle for an asset at a UE content path',
      mimeType: JSON_MIME,
    },
  ],
};

/** The two URIs the native handler serves bounded real data from the socket thread. */
const SOCKET_READABLE = new Set(['ue://capability/catalog', 'ue://project']);
const LISTED_URIS = new Set(NATIVE_LIST.resources.map((entry) => entry.uri));
const TEMPLATE_PREFIXES = ['ue://capability/', 'ue://knowledge/', 'ue://object/', 'ue://asset/'];

function matchesTemplate(uri: string): boolean {
  return TEMPLATE_PREFIXES.some((prefix) => uri.startsWith(prefix) && uri.length > prefix.length);
}

// Only the KEY set of `data` is asserted by the parity comparator; the values are
// representative of the native BuildReadBodyText output (bounded, redacted).
function readBody(uri: string): string {
  const data =
    uri === 'ue://project'
      ? { projectName: 'ParityProject', engineVersion: '5.7', contentRoot: '/Game', connected: true }
      : { capabilities: ['asset.list', 'asset.import'], count: 2, totalCount: 2, truncated: false };
  return JSON.stringify({ revision: 1, data });
}

/**
 * Model native `resources/read`. The two socket-readable URIs return a bounded
 * revisioned data body; a listed static resource or template-instance uri that is
 * not socket-readable returns RESOURCE_UNAVAILABLE; anything else returns the
 * distinct RESOURCE_NOT_FOUND (the native handler now separates unknown from
 * editor-state, mirroring the TS ResourceReadRouter).
 */
export function nativeRead(uri: string): RawReadResult | RawResourceError {
  if (SOCKET_READABLE.has(uri)) {
    return { contents: [{ uri, mimeType: JSON_MIME, revision: 1, text: readBody(uri) }] };
  }
  if (LISTED_URIS.has(uri) || matchesTemplate(uri)) {
    return {
      code: 'RESOURCE_UNAVAILABLE',
      uri,
      message: 'RESOURCE_UNAVAILABLE: editor-state resource is not readable from the transport thread',
      jsonRpcCode: NATIVE_INVALID_REQUEST,
    };
  }
  return {
    code: 'RESOURCE_NOT_FOUND',
    uri,
    message: `RESOURCE_NOT_FOUND: unknown resource: ${uri}`,
    jsonRpcCode: NATIVE_INVALID_PARAMS,
  };
}

export function isNativeError(value: RawReadResult | RawResourceError): value is RawResourceError {
  return typeof (value as RawResourceError).code === 'string';
}
