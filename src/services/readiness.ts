// src/services/readiness.ts
// Task 47: readiness for the TypeScript surface.
//
// Readiness answers "can this server actually serve a request right now", and it
// is the conjunction of three independent facts:
//   registry  - the GENERATED capability/gateway registry loaded and is non-empty
//   transport - the automation bridge is connected
//   editor    - a health check has actually SUCCEEDED against the editor
//
// Every probe fails CLOSED. In particular the editor probe requires a positive
// observation rather than inferring health from "the socket is open": a server
// that has never completed a health check does not know the editor is up, and
// answering "ready" there is precisely the false green this exists to prevent.
//
// Reason codes are a CLOSED set. A probe never surfaces a thrown message,
// because those routinely carry filesystem paths.

import { getGatewayManifestTools } from '../gateway/gateway-manifest.js';
import { TELEMETRY_READINESS_COMPONENTS, type TelemetryReadinessComponent } from './telemetry-schema.js';

export const READINESS_REASONS = [
  'editor_error',
  'editor_unavailable',
  'registry_empty',
  'registry_load_failed',
  'transport_disconnected',
] as const;
export type ReadinessReason = (typeof READINESS_REASONS)[number];

export interface ReadinessProbeResult {
  readonly ok: boolean;
  readonly reason?: ReadinessReason;
}

export type ReadinessProbe = () => ReadinessProbeResult;

export type ReadinessProbes = Readonly<Record<TelemetryReadinessComponent, ReadinessProbe>>;

export interface ReadinessReport {
  readonly ready: boolean;
  readonly components: Readonly<Record<TelemetryReadinessComponent, boolean>>;
  readonly notReady: readonly ReadinessReason[];
}

const READY: ReadinessProbeResult = { ok: true };

/**
 * Evaluate all three probes. A probe that throws is treated as its component
 * failing, never as a pass — an exception is exactly the case where "assume ok"
 * would produce a green readiness for a broken server.
 */
export function evaluateReadiness(probes: ReadinessProbes): ReadinessReport {
  const components: Record<TelemetryReadinessComponent, boolean> = {
    editor: false,
    registry: false,
    transport: false,
  };
  const notReady: ReadinessReason[] = [];

  for (const component of TELEMETRY_READINESS_COMPONENTS) {
    let result: ReadinessProbeResult;
    try {
      result = probes[component]();
    } catch {
      result = { ok: false, reason: defaultReasonFor(component) };
    }
    components[component] = result.ok === true;
    if (!components[component]) {
      notReady.push(result.reason ?? defaultReasonFor(component));
    }
  }

  return {
    ready: TELEMETRY_READINESS_COMPONENTS.every((component) => components[component]),
    components,
    notReady,
  };
}

function defaultReasonFor(component: TelemetryReadinessComponent): ReadinessReason {
  if (component === 'registry') return 'registry_load_failed';
  if (component === 'transport') return 'transport_disconnected';
  return 'editor_unavailable';
}

/**
 * Probe the GENERATED registry. Defaults to the real generated gateway manifest
 * so the production wiring exercises the same code path the tests do.
 */
export function createRegistryProbe(load: () => readonly unknown[] = getGatewayManifestTools): ReadinessProbe {
  return () => {
    let tools: readonly unknown[];
    try {
      tools = load();
    } catch {
      // Deliberately swallowed: a loader error message can carry a path.
      return { ok: false, reason: 'registry_load_failed' };
    }
    if (!Array.isArray(tools) || tools.length === 0) {
      return { ok: false, reason: 'registry_empty' };
    }
    return READY;
  };
}

export interface TransportStatusSource {
  getStatus(): { connected: boolean };
}

export function createTransportProbe(source: TransportStatusSource): ReadinessProbe {
  return () => (source.getStatus().connected ? READY : { ok: false, reason: 'transport_disconnected' });
}

export interface EditorStatusSource {
  readonly metrics: { readonly connectionStatus: 'connected' | 'disconnected' | 'error' };
}

export function createEditorProbe(source: EditorStatusSource): ReadinessProbe {
  return () => {
    const status = source.metrics.connectionStatus;
    if (status === 'connected') return READY;
    return { ok: false, reason: status === 'error' ? 'editor_error' : 'editor_unavailable' };
  };
}

export interface DefaultReadinessDependencies {
  readonly automationBridge: TransportStatusSource;
  readonly healthMonitor: EditorStatusSource;
  readonly loadRegistry?: () => readonly unknown[];
}

export function createDefaultReadinessProbes(deps: DefaultReadinessDependencies): ReadinessProbes {
  return {
    registry: deps.loadRegistry ? createRegistryProbe(deps.loadRegistry) : createRegistryProbe(),
    transport: createTransportProbe(deps.automationBridge),
    editor: createEditorProbe(deps.healthMonitor),
  };
}
