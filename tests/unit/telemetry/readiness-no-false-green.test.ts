// Task 47 — readiness must reflect generated-registry + transport + editor state
// with NO false green.
//
// The defect class this guards is an endpoint that answers 200/"ok" regardless
// of whether the server can actually serve a request. Every assertion below
// drives ONE dependency into failure and requires readiness to go FALSE through
// the real HTTP surface, not through an internal flag.

import http from 'node:http';
import net from 'node:net';

import { afterEach, describe, expect, it } from 'vitest';

import { HealthMonitor } from '../../../src/services/health-monitor.js';
import {
  createRegistryProbe,
  evaluateReadiness,
  type ReadinessProbes,
} from '../../../src/services/readiness.js';
import { startMetricsServer } from '../../../src/services/metrics-server.js';
import { TELEMETRY_METRIC_NAMES } from '../../../src/services/telemetry-schema.js';
import { Logger } from '../../../src/utils/logging/logger.js';

const originalPort = process.env.MCP_METRICS_PORT;
const originalPrometheusPort = process.env.PROMETHEUS_PORT;
const originalHost = process.env.MCP_METRICS_HOST;
const originalToken = process.env.MCP_METRICS_TOKEN;

afterEach(() => {
  const restore = (key: string, value: string | undefined) => {
    if (value === undefined) delete process.env[key];
    else process.env[key] = value;
  };
  restore('MCP_METRICS_PORT', originalPort);
  restore('PROMETHEUS_PORT', originalPrometheusPort);
  restore('MCP_METRICS_HOST', originalHost);
  restore('MCP_METRICS_TOKEN', originalToken);
});

const okProbe = () => ({ ok: true } as const);

function probes(overrides: Partial<ReadinessProbes> = {}): ReadinessProbes {
  return {
    registry: okProbe,
    transport: okProbe,
    editor: okProbe,
    ...overrides,
  };
}

async function availablePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      if (!address || typeof address === 'string') {
        server.close(() => reject(new Error('Failed to allocate a TCP port')));
        return;
      }
      const { port } = address;
      server.close(() => resolve(port));
    });
  });
}

interface HttpResult {
  readonly status: number | undefined;
  readonly body: string;
}

async function get(port: number, path: string): Promise<HttpResult> {
  return new Promise((resolve, reject) => {
    const req = http.get({ host: '127.0.0.1', port, path }, (res) => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk: string) => {
        body += chunk;
      });
      res.on('end', () => resolve({ status: res.statusCode, body }));
    });
    req.on('error', reject);
  });
}

function metricsOptions(readiness: ReadinessProbes, connected = true) {
  return {
    healthMonitor: new HealthMonitor(new Logger('ReadinessTest', 'error')),
    automationBridge: {
      getStatus: () => ({
        connected,
        pendingRequests: 0,
        maxPendingRequests: 10,
        maxConcurrentConnections: 1,
      }),
    },
    logger: new Logger('ReadinessTest', 'error'),
    readiness,
  };
}

async function withServer(
  readiness: ReadinessProbes,
  run: (port: number) => Promise<void>,
): Promise<void> {
  const port = await availablePort();
  process.env.MCP_METRICS_PORT = String(port);
  process.env.MCP_METRICS_HOST = '127.0.0.1';
  delete process.env.PROMETHEUS_PORT;
  delete process.env.MCP_METRICS_TOKEN;

  const server = startMetricsServer(metricsOptions(readiness));
  expect(server).not.toBeNull();
  if (!server) return;
  try {
    await new Promise<void>((resolve) => server.once('listening', resolve));
    await run(port);
  } finally {
    await new Promise<void>((resolve, reject) => {
      server.close((error) => (error ? reject(error) : resolve()));
    });
  }
}

describe('Task 47 readiness evaluation', () => {
  it('is ready only when registry, transport and editor are all ok', () => {
    expect(evaluateReadiness(probes()).ready).toBe(true);
    expect(evaluateReadiness(probes({ registry: () => ({ ok: false, reason: 'registry_load_failed' }) })).ready).toBe(false);
    expect(evaluateReadiness(probes({ transport: () => ({ ok: false, reason: 'transport_disconnected' }) })).ready).toBe(false);
    expect(evaluateReadiness(probes({ editor: () => ({ ok: false, reason: 'editor_unavailable' }) })).ready).toBe(false);
  });

  it('reports a generated-registry load failure as NOT ready with a bounded reason', () => {
    const probe = createRegistryProbe(() => {
      throw new Error('ENOENT: /home/xav/secret/gateway-manifest.generated.js not found');
    });
    const report = evaluateReadiness(probes({ registry: probe }));

    expect(report.ready).toBe(false);
    expect(report.components.registry).toBe(false);
    expect(report.notReady).toContain('registry_load_failed');
    // The thrown text carries a filesystem path; it must not survive into the report.
    expect(JSON.stringify(report)).not.toContain('/home/xav');
    expect(JSON.stringify(report)).not.toContain('ENOENT');
  });

  it('treats an EMPTY generated registry as not ready (loading is not serving)', () => {
    const report = evaluateReadiness(probes({ registry: createRegistryProbe(() => []) }));
    expect(report.ready).toBe(false);
    expect(report.notReady).toContain('registry_empty');
  });

  it('treats a populated generated registry as ready', () => {
    const report = evaluateReadiness(probes({ registry: createRegistryProbe(() => [{ name: 'unreal' }]) }));
    expect(report.ready).toBe(true);
    expect(report.notReady).toEqual([]);
  });

  it('defaults to the REAL generated gateway registry when no loader is injected', () => {
    expect(createRegistryProbe()().ok).toBe(true);
  });
});

describe('Task 47 readiness over the metrics HTTP surface', () => {
  it('serves /ready 200 when every dependency is ok', async () => {
    await withServer(probes(), async (port) => {
      const result = await get(port, '/ready');
      expect(result.status).toBe(200);
      expect(JSON.parse(result.body)).toMatchObject({ ready: true });
    });
  });

  it('does NOT false-green /ready or /health when the generated registry fails to load', async () => {
    const failing = probes({
      registry: createRegistryProbe(() => {
        throw new Error('generated registry unavailable');
      }),
    });

    await withServer(failing, async (port) => {
      const ready = await get(port, '/ready');
      expect(ready.status).toBe(503);
      expect(JSON.parse(ready.body)).toMatchObject({ ready: false });

      const health = await get(port, '/health');
      expect(health.status).toBe(503);
      const parsed = JSON.parse(health.body) as { status: string; ready: boolean };
      expect(parsed.ready).toBe(false);
      expect(parsed.status).not.toBe('ok');
    });
  });

  it('does NOT false-green /health when the transport is disconnected', async () => {
    const failing = probes({ transport: () => ({ ok: false, reason: 'transport_disconnected' }) });
    await withServer(failing, async (port) => {
      const health = await get(port, '/health');
      expect(health.status).toBe(503);
      expect(JSON.parse(health.body)).toMatchObject({ ready: false });
    });
  });

  it('keeps /metrics scrapeable while NOT ready and exports the readiness gauges', async () => {
    const failing = probes({ editor: () => ({ ok: false, reason: 'editor_unavailable' }) });
    await withServer(failing, async (port) => {
      const metrics = await get(port, '/metrics');
      expect(metrics.status).toBe(200);
      expect(metrics.body).toContain(`${TELEMETRY_METRIC_NAMES.ready} 0`);
      expect(metrics.body).toContain(`${TELEMETRY_METRIC_NAMES.readinessComponent}{component="registry"} 1`);
      expect(metrics.body).toContain(`${TELEMETRY_METRIC_NAMES.readinessComponent}{component="editor"} 0`);
    });
  });

  it('exports the transport-consistent telemetry families from /metrics', async () => {
    await withServer(probes(), async (port) => {
      const metrics = await get(port, '/metrics');
      expect(metrics.status).toBe(200);
      for (const name of Object.values(TELEMETRY_METRIC_NAMES)) {
        expect(metrics.body, `metric family missing from /metrics: ${name}`).toContain(`# TYPE ${name} `);
      }
    });
  });
});
