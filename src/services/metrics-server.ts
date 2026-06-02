import http from 'node:http';
import { timingSafeEqual } from 'node:crypto';
import { HealthMonitor } from './health-monitor.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

interface MetricsServerOptions {
  healthMonitor: HealthMonitor;
  automationBridge: AutomationBridge;
  logger: Logger;
}

function parseMetricsPort(value: string | undefined): number {
  if (!value || value.trim().length === 0) return 0;
  const trimmed = value.trim();
  if (!/^\d+$/.test(trimmed)) return 0;
  const port = Number(trimmed);
  return Number.isInteger(port) && port > 0 && port <= 65535 ? port : 0;
}

function isLoopbackHost(host: string): boolean {
  const normalized = host.trim().toLowerCase();
  return normalized === 'localhost' || normalized === '127.0.0.1' || normalized === '::1' || normalized === '[::1]';
}

function parseMetricsHost(value: string | undefined, logger: Logger): string | null {
  const host = value && value.trim().length > 0 ? value.trim() : '127.0.0.1';
  if (isLoopbackHost(host)) {
    if (host.toLowerCase() === 'localhost') return '127.0.0.1';
    if (host === '[::1]') return '::1';
    return host;
  }

  const allowNonLoopback = process.env.MCP_METRICS_ALLOW_NON_LOOPBACK?.toLowerCase() === 'true';
  if (!allowNonLoopback) {
    logger.warn(`Refusing to bind metrics server to non-loopback host '${host}'. Set MCP_METRICS_ALLOW_NON_LOOPBACK=true and MCP_METRICS_TOKEN to opt in.`);
    return null;
  }

  if (!process.env.MCP_METRICS_TOKEN?.trim()) {
    logger.warn('Refusing to expose metrics on a non-loopback host without MCP_METRICS_TOKEN.');
    return null;
  }

  return host;
}

function headerValue(value: string | string[] | undefined): string | undefined {
  return Array.isArray(value) ? value[0] : value;
}

function tokensMatch(candidate: string, expected: string): boolean {
  const candidateBytes = Buffer.from(candidate);
  const expectedBytes = Buffer.from(expected);
  return candidateBytes.length === expectedBytes.length && timingSafeEqual(candidateBytes, expectedBytes);
}

function isAuthorized(req: http.IncomingMessage, token: string): boolean {
  const explicitToken = headerValue(req.headers['x-mcp-metrics-token']);
  if (explicitToken && tokensMatch(explicitToken, token)) return true;

  const authHeader = headerValue(req.headers.authorization);
  const bearerPrefix = 'Bearer ';
  if (authHeader?.startsWith(bearerPrefix)) {
    return tokensMatch(authHeader.slice(bearerPrefix.length).trim(), token);
  }

  return false;
}

function formatPrometheusMetrics(options: MetricsServerOptions): string {
  const { healthMonitor, automationBridge } = options;
  const m = healthMonitor.metrics;
  const status = automationBridge.getStatus();

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
  const port = parseMetricsPort(portEnv);

  if (!port || !Number.isFinite(port) || port <= 0) {
    logger.debug('Metrics server disabled (set MCP_METRICS_PORT to enable Prometheus /metrics endpoint).');
    return null;
  }

  const host = parseMetricsHost(process.env.MCP_METRICS_HOST, logger);
  if (!host) {
    return null;
  }
  const metricsToken = process.env.MCP_METRICS_TOKEN?.trim();

  // Simple rate limiting: max 60 requests per minute per IP
  const requestCounts = new Map<string, { count: number; resetTime: number }>();
  const RATE_LIMIT = 60;
  const RATE_WINDOW_MS = 60_000;

  const server = http.createServer((req, res) => {
    const clientIp = req.socket.remoteAddress || 'unknown';

    // Rate limiting
    const now = Date.now();
    const clientData = requestCounts.get(clientIp);
    if (clientData && now < clientData.resetTime) {
      if (clientData.count >= RATE_LIMIT) {
        res.writeHead(429, { 'Content-Type': 'text/plain' });
        res.end('Too Many Requests');
        return;
      }
      clientData.count++;
    } else {
      requestCounts.set(clientIp, { count: 1, resetTime: now + RATE_WINDOW_MS });
    }

    // Clean up old entries
    for (const [ip, data] of requestCounts) {
      if (now > data.resetTime) {
        requestCounts.delete(ip);
      }
    }

    if (metricsToken && !isAuthorized(req, metricsToken)) {
      res.writeHead(401, { 'Content-Type': 'text/plain' });
      res.end('Unauthorized');
      return;
    }

    if (req.url === '/metrics' && req.method === 'GET') {
      const metrics = formatPrometheusMetrics(options);
      res.writeHead(200, { 'Content-Type': 'text/plain; version=0.0.4' });
      res.end(metrics);
    } else if (req.url === '/health' && req.method === 'GET') {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ status: 'ok', uptime: Date.now() - options.healthMonitor.metrics.uptime }));
    } else {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('Not Found');
    }
  });

  server.listen(port, host, () => {
    logger.info(`Metrics server listening on http://${host}:${port}/metrics`);
  });

  server.on('error', (error) => {
    const err = error as NodeJS.ErrnoException;
    if (err.code === 'EADDRINUSE') {
      logger.warn(`Port ${port} is already in use, metrics server not started`);
    } else {
      logger.error('Metrics server error:', error);
    }
  });

  return server;
}
