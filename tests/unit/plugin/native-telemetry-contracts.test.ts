// Task 47 — native telemetry source contracts.
//
// The native surface must export REAL counters/histograms, not log-only
// telemetry. These assertions read the plugin source text (what the compiler
// sees) so a metric that exists only in a comment or a header type cannot pass.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PLUGIN = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const SCHEMA_HEADER = resolve(PLUGIN, 'Private/Foundation/McpTelemetrySchema.h');
const REGISTRY_HEADER = resolve(PLUGIN, 'Private/Foundation/McpTelemetryRegistry.h');
const REGISTRY_SOURCE = resolve(PLUGIN, 'Private/Foundation/McpTelemetryRegistry.cpp');
const REGISTRY_RENDER = resolve(PLUGIN, 'Private/Foundation/McpTelemetryRegistryRender.cpp');
const CONNECTION_TELEMETRY = resolve(PLUGIN, 'Private/Transport/Connection/McpConnectionManagerTelemetry.cpp');
const PROCESS_REQUEST = resolve(PLUGIN, 'Private/Core/Requests/McpAutomationBridge_ProcessRequest.cpp');
const REQUEST_QUEUE = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemRequestQueue.cpp');
const REGISTRY_TESTS = resolve(PLUGIN, 'Private/Tests/McpTelemetryRegistryTests.cpp');
const READINESS_STATE = resolve(PLUGIN, 'Private/Foundation/McpReadinessState.h');
const HEALTH_CONTENT = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceHealthContent.cpp');
const READ_CONTENT = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceReadContent.cpp');
const RESOURCE_CATALOG = resolve(PLUGIN, 'Private/MCP/Resources/McpResourceCatalog.h');
const TRANSPORT_PRIMITIVES = resolve(PLUGIN, 'Private/MCP/Transport/McpNativeTransportPrimitives.cpp');
const SUBSYSTEM_LIFECYCLE = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp');
const HEALTH_READ_TESTS = resolve(PLUGIN, 'Private/Tests/Resources/McpHealthResourceReadTests.cpp');
const SUBSYSTEM_RESPONSES = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemResponses.cpp');

function read(path: string): string {
  expect(existsSync(path), `missing native file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

describe('Task 47 native telemetry exports real counters', () => {
  it('ships a schema mirror header with every declared region', () => {
    const source = read(SCHEMA_HEADER);
    for (const region of [
      'MetricNames',
      'LabelNames',
      'SurfaceValues',
      'ActionClassValues',
      'OutcomeValues',
      'FailureClassValues',
      'ReadinessComponentValues',
      'LatencyBuckets',
      'Quantiles',
    ]) {
      expect(source).toContain(`MCP_TELEMETRY_SCHEMA_BEGIN ${region}`);
      expect(source).toContain(`MCP_TELEMETRY_SCHEMA_END ${region}`);
    }
  });

  it('ships a counter/histogram registry with an injectable clock', () => {
    const header = code(read(REGISTRY_HEADER));
    expect(header).toContain('class FMcpTelemetryRegistry');
    expect(header).toContain('ObserveRequest');
    expect(header).toContain('BeginRequest');
    expect(header).toContain('MarkDispatched');
    expect(header).toContain('EndRequest');
    expect(header).toContain('RenderPrometheus');
    expect(header).toContain('QuantileSeconds');
    // A fake clock is the only way the percentile/queue tests can be exact.
    expect(header).toMatch(/SetClock|ClockSeconds|TFunction<double\(\)>/);
  });

  it('accumulates counters and histogram buckets in the registry implementation', () => {
    const source = code(read(REGISTRY_SOURCE));
    expect(source).toContain('McpTelemetrySchema::');
    // Real accumulation, not a log line.
    expect(source).toMatch(/\+\+\s*\w*(Count|Total)|\w*(Count|Total)\s*\+=/);
    expect(source).toContain('BucketCounts');
    expect(source).toContain('SumSeconds');
  });

  it('records automation outcomes into the registry instead of only logging them', () => {
    const source = code(read(CONNECTION_TELEMETRY));
    expect(source).toContain('FMcpTelemetryRegistry::Get()');
    expect(source).toMatch(/ObserveRequest|EndRequest/);
  });

  it('records dispatch duration from the request path into the registry', () => {
    const source = code(read(PROCESS_REQUEST));
    expect(source).toContain('FMcpTelemetryRegistry::Get()');
    // The existing DurationMs computation must now feed a counter, not just UE_LOG.
    expect(source).toMatch(/Observe\w*|EndRequest/);
  });

  it('measures queue wait between admission and dispatch rather than estimating it', () => {
    const queue = code(read(REQUEST_QUEUE));
    // Both ends of the interval must exist in the queue translation unit: the
    // admission call opens it, the dispatch call closes it. One without the
    // other yields a queue-wait metric that is structurally always zero.
    expect(queue).toMatch(/FMcpTelemetryRegistry::Get\(\)\s*\.\s*BeginRequest/);
    expect(queue).toMatch(/FMcpTelemetryRegistry::Get\(\)\s*\.\s*MarkDispatched/);
    // The dispatch end must sit inside the drain, after the fair-batch
    // selection, not beside the admission call.
    expect(queue.indexOf('MarkDispatched')).toBeGreaterThan(queue.indexOf('BeginRequest'));
  });

  it('closes the queue interval at the real dispatch point, after the reentrancy guard', () => {
    const process = code(read(PROCESS_REQUEST));
    const guardIndex = process.indexOf('if (bProcessingAutomationRequest)');
    const markIndex = process.indexOf('MarkDispatched');
    expect(guardIndex).toBeGreaterThanOrEqual(0);
    expect(markIndex).toBeGreaterThan(guardIndex);
  });

  it('never uses a request id, action name or path as a metric label', () => {
    const registry = code(read(REGISTRY_SOURCE)) + code(read(REGISTRY_HEADER));
    // Label values may only come from the bounded schema coercion helpers.
    expect(registry).toMatch(/CoerceActionClass|NormalizeActionClass/);
    expect(registry).toMatch(/CoerceFailureClass|NormalizeFailureClass/);

    // The exposition writer is the only place a label VALUE is formatted, so it
    // is checked in full rather than from an offset.
    const render = code(read(REGISTRY_RENDER));
    for (const forbidden of ['RequestId', 'Message', 'CapabilityId']) {
      expect(render.includes(forbidden), `raw identifier reachable in metric text: ${forbidden}`).toBe(false);
    }
  });

  it('ships a native registry test that drives the fake clock', () => {
    const tests = code(read(REGISTRY_TESTS));
    expect(tests).toContain('FMcpTelemetryRegistry');
    expect(tests).toMatch(/SetClock|ClockSeconds/);
    expect(tests).toContain('QuantileSeconds');
  });

  it('keeps every new native telemetry file within the 250 pure-line ceiling', () => {
    const pureLines = (source: string): number =>
      source
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .split('\n')
        .map((line) => line.trim())
        .filter((line) => line.length > 0 && !line.startsWith('//') && !line.startsWith('#')).length;

    for (const path of [
      SCHEMA_HEADER,
      REGISTRY_HEADER,
      REGISTRY_SOURCE,
      REGISTRY_RENDER,
      REGISTRY_TESTS,
      READINESS_STATE,
      HEALTH_CONTENT,
      READ_CONTENT,
      RESOURCE_CATALOG,
      HEALTH_READ_TESTS,
    ]) {
      expect(pureLines(read(path)), `${path} exceeds the 250 pure-line ceiling`).toBeLessThanOrEqual(250);
    }
  });
});

describe('Task 47 native telemetry is client-readable through ue://health', () => {
  it('classifies ue://health as socket-readable instead of refusing it as editor state', () => {
    const catalog = code(read(RESOURCE_CATALOG));
    // ue://health is socket-readable because it is ADVERTISED and Classify sends
    // every advertised uri down the SocketReadable branch. Asserting that
    // generalised property, rather than grepping Classify for the literal
    // HealthUri(), keeps this contract true through the refactor that made the
    // advertised set and the readable set one and the same list.
    const unserved = catalog.slice(catalog.indexOf('NativeUnservedUris'), catalog.indexOf('IsNativeUnservedUri'));
    expect(unserved).not.toContain('ue://health');
    expect(catalog.slice(catalog.indexOf('LegacyStaticResources'))).toContain('HealthUri()');

    const source = code(read(READ_CONTENT));
    const classify = source.slice(source.indexOf('EReadKind Classify'), source.indexOf('FString UnavailableMessage'));
    expect(classify).toContain('IsListedResourceUri');
    expect(classify.indexOf('IsListedResourceUri')).toBeLessThan(classify.indexOf('EditorUnavailable'));
    expect(classify.indexOf('SocketReadable')).toBeLessThan(classify.indexOf('EditorUnavailable'));
  });

  it('routes the health read to the telemetry-backed body builder', () => {
    const source = code(read(READ_CONTENT));
    expect(source).toContain('McpResourceHealth::BuildHealthData()');
    expect(source).toContain('#include "MCP/Resources/McpResourceHealthContent.h"');
  });

  it('serves the rendered exposition and the anonymous aggregates, not just one of them', () => {
    const source = code(read(HEALTH_CONTENT));
    expect(source).toContain('RenderPrometheus');
    expect(source).toContain('SnapshotJson');
    expect(source).toContain('metricsExposition');
    expect(source).toContain('diagnostics');
    expect(source).toContain('readiness');
    // The readiness view must actually be passed, otherwise the two readiness
    // families render their headers with no samples underneath.
    expect(source).toMatch(/RenderPrometheus\(\s*&\s*View\s*\)/);
  });

  it('keeps the health body free of any unbounded dimension', () => {
    const source = code(read(HEALTH_CONTENT));
    for (const forbidden of ['RequestId', 'CapabilityToken', 'GetStatusDetail']) {
      expect(source.includes(forbidden), `unbounded value reachable in the served body: ${forbidden}`).toBe(false);
    }
  });

  it('publishes readiness fail-closed from the layer that owns each fact', () => {
    const state = code(read(READINESS_STATE));
    // Both flags must start false; a default-true flag is a false green.
    expect(state).toMatch(/bTransportReady\s*\{\s*false\s*\}/);
    expect(state).toMatch(/bEditorReady\s*\{\s*false\s*\}/);

    const lifecycle = code(read(SUBSYSTEM_LIFECYCLE));
    expect(lifecycle).toContain('SetTransportReady(true)');
    expect(lifecycle).toContain('SetEditorReady(true)');
    // Editor readiness is published only after handlers and the ticker exist.
    expect(lifecycle.indexOf('SetEditorReady(true)')).toBeGreaterThan(lifecycle.indexOf('InitializeHandlers()'));
    expect(lifecycle.indexOf('FMcpReadinessState::Get().Reset()')).toBeGreaterThanOrEqual(0);
  });

  it('keeps the transport resources/read branch on Classify + BuildReadBody', () => {
    const source = code(read(TRANSPORT_PRIMITIVES));
    const branch = source.slice(source.indexOf('resources/read'), source.indexOf('resources/subscribe'));
    expect(branch).toContain('McpResourceRead::Classify(Uri)');
    expect(branch).toContain('McpResourceRead::BuildReadBody(Uri');
    // No health special-case in the transport: the read path stays uniform, so
    // the automation test that drives Classify + BuildReadBody is the real path.
    expect(branch).not.toContain('health');
  });

  it('proves reachability through the read path rather than by calling the renderer', () => {
    const tests = code(read(HEALTH_READ_TESTS));
    expect(tests).toContain('McpResourceRead::Classify');
    expect(tests).toContain('McpResourceRead::BuildReadBody');
    // Calling RenderPrometheus here would re-prove the renderer, which was
    // already true while no client could reach it. That is the whole gap.
    expect(tests.includes('RenderPrometheus'), 'the read-path test must not call the renderer directly').toBe(false);
    expect(tests).toContain('EveryServedLabelValueIsBounded');
  });

  it('closes the telemetry interval on the native reply path, which returns before the connection manager', () => {
    const source = code(read(SUBSYSTEM_RESPONSES));
    const branchStart = source.indexOf('EffectiveOrigin == ERequestOrigin::NativeHTTP');
    expect(branchStart).toBeGreaterThanOrEqual(0);
    const nativeBranch = source.slice(
      branchStart,
      source.indexOf('ConnectionManager->SendAutomationResponse', branchStart),
    );
    // The native branch RETURNS, so the connection-manager EndRequest can never
    // run for it. Without a terminal here the interval opened at queue
    // admission is never closed and a native-only deployment scrapes zeros.
    expect(nativeBranch).toMatch(/FMcpTelemetryRegistry::Get\(\)\s*\.\s*EndRequest/);
    expect(nativeBranch.indexOf('EndRequest')).toBeLessThan(nativeBranch.indexOf('CompletePendingRequest'));
    // Only the bounded error CODE may be forwarded as the failure class.
    const call = nativeBranch.slice(nativeBranch.indexOf('EndRequest'), nativeBranch.indexOf('CompletePendingRequest'));
    expect(call).toContain('EffectiveErrorCode');
    expect(call).not.toContain('EffectiveMessage');
  });

  it('names the health uri once so catalog, classifier and body builder cannot drift', () => {
    // Read RAW here: comment stripping would eat the `//` inside a `ue://` uri.
    const catalog = read(RESOURCE_CATALOG);
    expect(catalog).toContain('inline const FString& HealthUri()');
    expect(catalog).toContain('{ HealthUri(), TEXT("Health Status")');
    expect(catalog.match(/ue:\/\/health/gu)?.length).toBe(1);
  });
});
