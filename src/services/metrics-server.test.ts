import http from 'node:http';
import net from 'node:net';
import { afterEach, describe, expect, it } from 'vitest';
import { startMetricsServer } from './metrics-server.js';
import { HealthMonitor } from './health-monitor.js';
import { Logger } from '../utils/logger.js';
import type { AutomationBridge } from '../automation/index.js';

const originalMetricsPort = process.env.MCP_METRICS_PORT;
const originalPrometheusPort = process.env.PROMETHEUS_PORT;
const originalMetricsHost = process.env.MCP_METRICS_HOST;
const originalMetricsAllowNonLoopback = process.env.MCP_METRICS_ALLOW_NON_LOOPBACK;
const originalMetricsToken = process.env.MCP_METRICS_TOKEN;

afterEach(() => {
  if (originalMetricsPort === undefined) {
    delete process.env.MCP_METRICS_PORT;
  } else {
    process.env.MCP_METRICS_PORT = originalMetricsPort;
  }

  if (originalPrometheusPort === undefined) {
    delete process.env.PROMETHEUS_PORT;
  } else {
    process.env.PROMETHEUS_PORT = originalPrometheusPort;
  }

  if (originalMetricsHost === undefined) {
    delete process.env.MCP_METRICS_HOST;
  } else {
    process.env.MCP_METRICS_HOST = originalMetricsHost;
  }

  if (originalMetricsAllowNonLoopback === undefined) {
    delete process.env.MCP_METRICS_ALLOW_NON_LOOPBACK;
  } else {
    process.env.MCP_METRICS_ALLOW_NON_LOOPBACK = originalMetricsAllowNonLoopback;
  }

  if (originalMetricsToken === undefined) {
    delete process.env.MCP_METRICS_TOKEN;
  } else {
    process.env.MCP_METRICS_TOKEN = originalMetricsToken;
  }
});

function createOptions() {
  return {
    healthMonitor: new HealthMonitor(new Logger('MetricsServerTest', 'error')),
    automationBridge: {
      getStatus: () => ({
        connected: false,
        pendingRequests: 0,
        maxPendingRequests: 10,
        maxConcurrentConnections: 1
      })
    } as unknown as AutomationBridge,
    logger: new Logger('MetricsServerTest', 'error')
  };
}

async function getAvailablePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      if (!address || typeof address === 'string') {
        server.close(() => reject(new Error('Failed to allocate a TCP port')));
        return;
      }

      const port = address.port;
      server.close(() => resolve(port));
    });
  });
}

async function getStatusCode(port: number, headers: Record<string, string> = {}): Promise<number | undefined> {
  return new Promise((resolve, reject) => {
    const req = http.get({ host: '127.0.0.1', port, path: '/metrics', headers }, res => {
      res.resume();
      res.on('end', () => resolve(res.statusCode));
    });
    req.on('error', reject);
  });
}

async function closeServer(server: http.Server): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    server.close(error => {
      if (error) reject(error);
      else resolve();
    });
  });
}

describe('startMetricsServer', () => {
  it('rejects partial numeric metrics ports', () => {
    process.env.MCP_METRICS_PORT = '8091abc';
    delete process.env.PROMETHEUS_PORT;

    const server = startMetricsServer(createOptions());

    expect(server).toBeNull();
  });

  it('rejects fractional metrics ports', () => {
    process.env.MCP_METRICS_PORT = '8091.5';
    delete process.env.PROMETHEUS_PORT;

    const server = startMetricsServer(createOptions());

    expect(server).toBeNull();
  });

  it('rejects non-decimal metrics ports', () => {
    process.env.MCP_METRICS_PORT = '0x1f9b';
    delete process.env.PROMETHEUS_PORT;

    const server = startMetricsServer(createOptions());

    expect(server).toBeNull();
  });

  it('refuses non-loopback metrics hosts without explicit authenticated opt-in', () => {
    process.env.MCP_METRICS_PORT = '8091';
    process.env.MCP_METRICS_HOST = '0.0.0.0';
    delete process.env.MCP_METRICS_ALLOW_NON_LOOPBACK;
    delete process.env.MCP_METRICS_TOKEN;

    const server = startMetricsServer(createOptions());

    expect(server).toBeNull();
  });

  it('refuses non-loopback metrics hosts without a metrics token', () => {
    process.env.MCP_METRICS_PORT = '8091';
    process.env.MCP_METRICS_HOST = '0.0.0.0';
    process.env.MCP_METRICS_ALLOW_NON_LOOPBACK = 'true';
    delete process.env.MCP_METRICS_TOKEN;

    const server = startMetricsServer(createOptions());

    expect(server).toBeNull();
  });

  it('requires the configured metrics token before serving metrics', async () => {
    const port = await getAvailablePort();
    process.env.MCP_METRICS_PORT = String(port);
    process.env.MCP_METRICS_HOST = '127.0.0.1';
    process.env.MCP_METRICS_TOKEN = 'qa-token';

    const server = startMetricsServer(createOptions());
    expect(server).not.toBeNull();
    if (!server) return;

    try {
      await new Promise<void>(resolve => server.once('listening', resolve));

      await expect(getStatusCode(port)).resolves.toBe(401);
      await expect(getStatusCode(port, { Authorization: 'Bearer qa-token' })).resolves.toBe(200);
      await expect(getStatusCode(port, { 'X-MCP-Metrics-Token': 'qa-token' })).resolves.toBe(200);
    } finally {
      await closeServer(server);
    }
  });

  it('rate limits unauthorized metrics traffic before token validation', async () => {
    const port = await getAvailablePort();
    process.env.MCP_METRICS_PORT = String(port);
    process.env.MCP_METRICS_HOST = '127.0.0.1';
    process.env.MCP_METRICS_TOKEN = 'qa-token';

    const server = startMetricsServer(createOptions());
    expect(server).not.toBeNull();
    if (!server) return;

    try {
      await new Promise<void>(resolve => server.once('listening', resolve));

      for (let i = 0; i < 60; i += 1) {
        await expect(getStatusCode(port)).resolves.toBe(401);
      }
      await expect(getStatusCode(port)).resolves.toBe(429);
    } finally {
      await closeServer(server);
    }
  });
});
