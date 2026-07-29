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
//   resources/list        -> McpResourceCatalog::AllListedResources() (6 legacy + 5 new = 11)
//   resources/templates   -> McpResourceCatalog::Templates()          (4 defs)
//   resources/read ok      -> McpResourceRead::BuildReadBodyText for the four socket-readable
//                            URIs (ue://capability/catalog, ue://project, ue://state/revisions,
//                            and since Task 47 ue://health): a bounded
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

/**
 * native resources/list — the SIX resources native both advertises and serves.
 *
 * ue://assets, ue://actors, ue://level, ue://editor and ue://selection are
 * deliberately absent: each is live editor state the socket thread may not read,
 * and native no longer advertises what it would refuse. See
 * McpResourceCatalog::NativeUnservedUris().
 */
export const NATIVE_LIST: { readonly resources: readonly RawResourceEntry[] } = {
  resources: [
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
    {
      uri: 'ue://state/revisions',
      name: 'Live State Revisions',
      description: 'Current selection, level, asset-registry, and package revision counters',
      mimeType: JSON_MIME,
    },
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

/**
 * Every advertised uri is socket-readable, which is the invariant the native
 * catalogue now enforces by construction: SOCKET_READABLE is derived from the
 * advertised list rather than maintained beside it, so the two cannot drift.
 */
const SOCKET_READABLE = new Set(NATIVE_LIST.resources.map((entry) => entry.uri));

/** Served by the stdio transport, deliberately not advertised by native. */
export const NATIVE_UNSERVED_URIS = ['ue://assets', 'ue://actors', 'ue://level', 'ue://editor', 'ue://selection'] as const;
const TEMPLATE_PREFIXES = ['ue://capability/', 'ue://knowledge/', 'ue://object/', 'ue://asset/'];

function matchesTemplate(uri: string): boolean {
  return TEMPLATE_PREFIXES.some((prefix) => uri.startsWith(prefix) && uri.length > prefix.length);
}

// Only the KEY set of `data` is asserted by the parity comparator; the values are
// representative of the native BuildReadBodyText output (bounded, redacted).
/**
 * Native `ue://health` exposition, transcribed from the line order
 * `FMcpTelemetryRegistry::RenderPrometheus` emits (family headers are always
 * present, readiness gauges follow) for an idle registry with all three
 * components ready.
 */
export const NATIVE_HEALTH_EXPOSITION = [
  '# TYPE unreal_mcp_request_duration_seconds histogram',
  '# TYPE unreal_mcp_request_duration_quantile_seconds gauge',
  '# TYPE unreal_mcp_queue_wait_seconds histogram',
  '# TYPE unreal_mcp_queue_wait_quantile_seconds gauge',
  '# TYPE unreal_mcp_requests_by_class_total counter',
  '# TYPE unreal_mcp_failures_by_class_total counter',
  '# TYPE unreal_mcp_readiness_component gauge',
  'unreal_mcp_readiness_component{component="editor"} 1',
  'unreal_mcp_readiness_component{component="registry"} 1',
  'unreal_mcp_readiness_component{component="transport"} 1',
  '# TYPE unreal_mcp_ready gauge',
  'unreal_mcp_ready 1',
].join('\n') + '\n';

/**
 * Native `ue://health` data, transcribed from
 * `Private/MCP/Resources/McpResourceHealthContent.cpp` BuildHealthData().
 */
export const NATIVE_HEALTH_DATA = {
  surface: 'native',
  readiness: { ready: true, components: { editor: true, registry: true, transport: true }, notReady: [] },
  diagnostics: {
    totals: { requests: 0, failures: 0 },
    byActionClass: [],
    byFailureClass: [],
    queueWait: { p50Seconds: null, p95Seconds: null },
  },
  metricsExposition: NATIVE_HEALTH_EXPOSITION,
} as const;

/**
 * Native `ue://version`, transcribed from
 * `Private/MCP/Resources/McpResourceBridgeContent.cpp` BuildEngineVersionData().
 * Same key set as the TypeScript EngineVersionInfo.
 */
export const NATIVE_ENGINE_VERSION_DATA = {
  version: '5.7.4', major: 5, minor: 7, patch: 4, isUE56OrAbove: true,
} as const;

/**
 * Native `ue://automation-bridge`, transcribed from the same file's
 * BuildAutomationBridgeData(). The timestamp fields are null because the plugin
 * is the SERVER and holds no client-side connection history; the key set is kept
 * identical to the TypeScript body so one parser handles both transports.
 */
export const NATIVE_AUTOMATION_BRIDGE_DATA = {
  summary: {
    enabled: true, connected: true, host: '127.0.0.1', port: 8090,
    capabilityTokenRequired: false, pendingRequests: 0,
  },
  timestamps: { connectedAt: null, lastHandshakeAt: null, lastMessageAt: null, lastRequestSentAt: null },
  lastDisconnect: null,
  lastHandshakeFailure: null,
  lastError: null,
  listening: true,
} as const;

function readBody(uri: string): string {
  const data =
    uri === 'ue://project'
      ? { projectName: 'ParityProject', engineVersion: '5.7', contentRoot: '/Game', connected: true }
      : uri === 'ue://state/revisions'
        ? { selection: 1, level: 1, assetRegistry: 1, package: 1 }
      : uri === 'ue://health'
        ? NATIVE_HEALTH_DATA
      : uri === 'ue://version'
        ? NATIVE_ENGINE_VERSION_DATA
      : uri === 'ue://automation-bridge'
        ? NATIVE_AUTOMATION_BRIDGE_DATA
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
  if (NATIVE_UNSERVED_URIS.includes(uri as (typeof NATIVE_UNSERVED_URIS)[number])) {
    return {
      code: 'RESOURCE_UNAVAILABLE',
      uri,
      message: `RESOURCE_UNAVAILABLE: ${uri} is live editor state that only the game thread can read, so the `
        + 'native /mcp transport does not advertise or serve it; read it over the stdio transport',
      jsonRpcCode: NATIVE_INVALID_REQUEST,
    };
  }
  if (matchesTemplate(uri)) {
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
