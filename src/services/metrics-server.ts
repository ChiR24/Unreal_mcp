import http from 'http';
import { HealthMonitor } from './health-monitor.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';
import { wasmIntegration } from '../wasm/index.js';
import { DEFAULT_AUTOMATION_HOST } from '../constants.js';

interface MetricsServerOptions {
  healthMonitor: HealthMonitor;
  automationBridge: AutomationBridge;
  logger: Logger;
}

function formatPrometheusMetrics(options: MetricsServerOptions): string {
  const { healthMonitor, automationBridge } = options;
  const m = healthMonitor.metrics;
  const status = automationBridge.getStatus();
  const wasmMetrics = wasmIntegration.getMetrics();
  const wasmStatus = wasmIntegration.getStatus();

  const lines: string[] = [];

  // Basic request counters
  lines.push('# HELP unreal_mcp_requests_total Total number of tool requests seen by the MCP server.');
  lines.push('# TYPE unreal_mcp_requests_total counter');
  lines.push(`unreal_mcp_requests_total ${m.totalRequests}`);

  lines.push('# HELP unreal_mcp_requests_success_total Total number of successful tool requests.');
  lines.push('# TYPE unreal_mcp_requests_success_total counter');
  lines.push(`unreal_mcp_requests_success_total ${m.successfulRequests}`);

  lines.push('# HELP unreal_mcp_requests_failed_total Total number of failed tool requests.');
  lines.push('# TYPE unreal_mcp_requests_failed_total counter');
  lines.push(`unreal_mcp_requests_failed_total ${m.failedRequests}`);

  // Response time summary (simple gauges)
  lines.push('# HELP unreal_mcp_average_response_time_ms Average response time of recent tool requests (ms).');
  lines.push('# TYPE unreal_mcp_average_response_time_ms gauge');
  lines.push(`unreal_mcp_average_response_time_ms ${Number.isFinite(m.averageResponseTime) ? m.averageResponseTime.toFixed(2) : '0'}`);

  // Connection status gauges
  lines.push('# HELP unreal_mcp_unreal_connected Whether the Unreal automation bridge is currently connected (1) or not (0).');
  lines.push('# TYPE unreal_mcp_unreal_connected gauge');
  lines.push(`unreal_mcp_unreal_connected ${status.connected ? 1 : 0}`);

  lines.push('# HELP unreal_mcp_automation_pending_requests Number of pending automation bridge requests.');
  lines.push('# TYPE unreal_mcp_automation_pending_requests gauge');
  lines.push(`unreal_mcp_automation_pending_requests ${status.pendingRequests}`);

  lines.push('# HELP unreal_mcp_automation_max_pending_requests Configured maximum number of pending automation bridge requests.');
  lines.push('# TYPE unreal_mcp_automation_max_pending_requests gauge');
  lines.push(`unreal_mcp_automation_max_pending_requests ${status.maxPendingRequests}`);

  lines.push('# HELP unreal_mcp_automation_max_concurrent_connections Configured maximum concurrent automation bridge connections.');
  lines.push('# TYPE unreal_mcp_automation_max_concurrent_connections gauge');
  lines.push(`unreal_mcp_automation_max_concurrent_connections ${status.maxConcurrentConnections}`);

  // WASM integration metrics
  lines.push('# HELP unreal_mcp_wasm_enabled Whether the optional WebAssembly integration is enabled (1) or not (0).');
  lines.push('# TYPE unreal_mcp_wasm_enabled gauge');
  lines.push(`unreal_mcp_wasm_enabled ${wasmStatus.enabled ? 1 : 0}`);

  lines.push('# HELP unreal_mcp_wasm_ready Whether the WebAssembly module has been initialized successfully (1) or not (0).');
  lines.push('# TYPE unreal_mcp_wasm_ready gauge');
  lines.push(`unreal_mcp_wasm_ready ${wasmStatus.ready ? 1 : 0}`);

  lines.push('# HELP unreal_mcp_wasm_operations_total Total number of WASM-related operations recorded by the integration layer.');
  lines.push('# TYPE unreal_mcp_wasm_operations_total counter');
  lines.push(`unreal_mcp_wasm_operations_total ${wasmMetrics.totalOperations}`);

  lines.push('# HELP unreal_mcp_wasm_operations_wasm_total Number of operations that used the WebAssembly implementation.');
  lines.push('# TYPE unreal_mcp_wasm_operations_wasm_total counter');
  lines.push(`unreal_mcp_wasm_operations_wasm_total ${wasmMetrics.wasmOperations}`);

  lines.push('# HELP unreal_mcp_wasm_operations_ts_total Number of operations that used the TypeScript fallback implementation.');
  lines.push('# TYPE unreal_mcp_wasm_operations_ts_total counter');
  lines.push(`unreal_mcp_wasm_operations_ts_total ${wasmMetrics.tsOperations}`);

  // Uptime in seconds
  const uptimeSeconds = Math.floor((Date.now() - m.uptime) / 1000);
  lines.push('# HELP unreal_mcp_uptime_seconds MCP server uptime in seconds (since HealthMonitor was created).');
  lines.push('# TYPE unreal_mcp_uptime_seconds gauge');
  lines.push(`unreal_mcp_uptime_seconds ${uptimeSeconds}`);

  return lines.join('\n') + '\n';
}

export function startMetricsServer(options: MetricsServerOptions): http.Server | null {
  const { logger } = options;

  const portEnv = process.env.MCP_METRICS_PORT || process.env.PROMETHEUS_PORT;
  const port = portEnv ? Number(portEnv) : 0;

  if (!port || !Number.isFinite(port) || port <= 0) {
    logger.debug('Metrics server disabled (set MCP_METRICS_PORT to enable Prometheus /metrics endpoint).');
    return null;
  }

  // Simple rate limiting: max 60 requests per minute per IP
  const RATE_LIMIT_WINDOW_MS = 60000;
  const RATE_LIMIT_MAX_REQUESTS = 60;
  const requestCounts = new Map<string, { count: number; resetAt: number }>();

  function checkRateLimit(ip: string): boolean {
    const now = Date.now();
    const record = requestCounts.get(ip);

    if (!record || now >= record.resetAt) {
      requestCounts.set(ip, { count: 1, resetAt: now + RATE_LIMIT_WINDOW_MS });
      return true;
    }

    if (record.count >= RATE_LIMIT_MAX_REQUESTS) {
      return false;
    }

    record.count++;
    return true;
  }

  try {
    const server = http.createServer((req, res) => {
      if (!req.url) {
        res.statusCode = 400;
        res.end('Bad Request');
        return;
      }

      if (req.method !== 'GET') {
        res.statusCode = 405;
        res.setHeader('Allow', 'GET');
        res.end('Method Not Allowed');
        return;
      }

      if (!req.url.startsWith('/metrics')) {
        res.statusCode = 404;
        res.end('Not Found');
        return;
      }

      // Apply rate limiting
      const clientIp = req.socket.remoteAddress || 'unknown';
      if (!checkRateLimit(clientIp)) {
        res.statusCode = 429;
        res.setHeader('Retry-After', '60');
        res.end('Too Many Requests');
        return;
      }

      try {
        const body = formatPrometheusMetrics(options);
        res.statusCode = 200;
        res.setHeader('Content-Type', 'text/plain; version=0.0.4; charset=utf-8');
        res.end(body);
      } catch (err) {
        logger.warn('Failed to render /metrics payload', err as Error);
        res.statusCode = 500;
        res.end('Internal Server Error');
      }
    });

    server.listen(port, () => {
      logger.info(`Prometheus metrics server listening on http://${DEFAULT_AUTOMATION_HOST}:${port}/metrics`);
    });

    server.on('error', (err) => {
      logger.warn('Metrics server error', err as any);
    });

    return server;
  } catch (err) {
    logger.warn('Failed to start metrics server', err as any);
    return null;
  }
}
