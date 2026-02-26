import http from 'http';
import { HealthMonitor } from './health-monitor.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

interface MetricsServerOptions {
  healthMonitor: HealthMonitor;
  automationBridge: AutomationBridge;
  logger: Logger;
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
  const port = portEnv ? Number(portEnv) : 0;

  if (!port || !Number.isFinite(port) || port <= 0) {
    logger.debug('Metrics server disabled (set MCP_METRICS_PORT to enable Prometheus /metrics endpoint).');
    return null;
  }

  const host = process.env.MCP_METRICS_HOST || '127.0.0.1';

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
