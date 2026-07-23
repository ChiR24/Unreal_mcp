import { createRequire } from 'node:module';
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CancelledNotificationSchema } from '@modelcontextprotocol/sdk/types.js';

import { AutomationBridge } from '../automation/index.js';
import { AutomationLogger } from '../automation/log-redaction.js';
import { config } from '../config.js';
import { ServerSetup } from '../server-setup.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { startMetricsServer } from '../services/metrics-server.js';
import { consolidatedToolDefinitions } from '../tools/catalog/consolidated-tool-definitions.js';
import { unrealGatewayToolDefinition } from '../tools/catalog/unreal-gateway-definition.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { canonicalizeMcpRequestId } from '../automation/request-context.js';
import { responseValidator } from '../utils/responses/response-validator.js';
import { wirePrimitives } from './mcp-primitives/primitive-wiring.js';

const require = createRequire(import.meta.url);
const packageInfo: { name?: string; version?: string } = (() => {
  try {
    return require('../../package.json');
  } catch (error) {
    const logger = new AutomationLogger('UE-MCP');
    logger.debug(
      'Unable to read package.json for server metadata',
      error instanceof Error ? error : String(error),
    );
    return {};
  }
})();

const SERVER_NAME =
  typeof packageInfo.name === 'string' && packageInfo.name.trim().length > 0
    ? packageInfo.name
    : 'unreal-engine-mcp';
const SERVER_VERSION =
  typeof packageInfo.version === 'string' && packageInfo.version.trim().length > 0
    ? packageInfo.version
    : '0.5.30';
const AUTOMATION_HEARTBEAT_MS = 15_000;

export const log = new AutomationLogger('UE-MCP');

export function routeStdoutLogsToStderr(): void {
  if (!config.MCP_ROUTE_STDOUT_LOGS) {
    return;
  }

  const writeToStderr = (...args: unknown[]): void => {
    const line = args
      .map((argument) =>
        typeof argument === 'string' ? argument : JSON.stringify(argument),
      )
      .join(' ');
    process.stderr.write(`${line}\n`);
  };

  console.log = writeToStderr;
  console.info = writeToStderr;
  if (typeof console.debug === 'function') {
    console.debug = writeToStderr;
  }
}

export function createServer() {
  const bridge = new UnrealBridge();
  const healthMonitor = new HealthMonitor(log);
  const automationBridge = new AutomationBridge({
    serverName: SERVER_NAME,
    serverVersion: SERVER_VERSION,
    heartbeatIntervalMs: AUTOMATION_HEARTBEAT_MS,
    clientMode: config.MCP_AUTOMATION_CLIENT_MODE,
  });
  bridge.setAutomationBridge(automationBridge);

  automationBridge.on('connected', ({ metadata, port, protocol }) => {
    log.info(
      `Automation bridge connected (port=${port}, protocol=${protocol ?? 'none'})`,
      metadata,
    );
  });
  automationBridge.on('disconnected', ({ code, reason, port, protocol }) => {
    log.info(
      `Automation bridge disconnected (code=${code}, reason=${reason || 'n/a'}, port=${port}, protocol=${protocol ?? 'none'})`,
    );
  });
  automationBridge.on('handshakeFailed', ({ reason, port }) => {
    log.warn(`Automation bridge handshake failed (port=${port}): ${reason}`);
  });
  automationBridge.on('message', (message) => {
    log.debug('Automation bridge inbound message', message);
  });
  automationBridge.on('error', (error) => {
    log.error('Automation bridge error', error);
  });

  const metricsServer = startMetricsServer({
    healthMonitor,
    automationBridge,
    logger: log,
  });

  log.debug('Initializing response validation...');
  const toolDefinitions = consolidatedToolDefinitions as Array<{
    name: string;
    outputSchema?: Record<string, unknown>;
  }>;
  for (const tool of toolDefinitions) {
    if (tool.outputSchema) {
      responseValidator.registerSchema(tool.name, tool.outputSchema);
    }
  }
  if (unrealGatewayToolDefinition.outputSchema) {
    responseValidator.registerSchema(unrealGatewayToolDefinition.name, unrealGatewayToolDefinition.outputSchema);
  }
  log.debug(
    `Registered ${responseValidator.getStats().totalSchemas} output schemas for validation`,
  );

  log.debug('Server starting without connecting to Unreal Engine');
  healthMonitor.metrics.connectionStatus = 'disconnected';

  // Task 37: advertise exactly the implemented session-profile primitives — the
  // single `unreal` tool, resources (with subscribe), prompts, and completions —
  // and nothing else. Capabilities are set at constructor time, before
  // serverSetup registers the backing handlers; wirePrimitives then fails closed
  // if any advertised capability lacks its handler. The single stable tool never
  // changes membership, so no *_list_changed member is declared. This shape
  // mirrors ADVERTISED_SESSION_CAPABILITIES and is kept an inline literal.
  const server = new Server(
    {
      name: SERVER_NAME,
      version: SERVER_VERSION,
    },
    {
      capabilities: {
        tools: {},
        resources: { subscribe: true },
        prompts: {},
        completions: {},
      },
    },
  );

  automationBridge.on('automationEvent', (event) => {
    server.notification({
      method: 'notifications/unreal/automation_event',
      params: event,
    }).catch((error: unknown) => {
      log.error(
        'Failed to forward Unreal automation event notification',
        error instanceof Error ? error : String(error),
      );
    });
  });

  const serverSetup = new ServerSetup(
    server,
    bridge,
    automationBridge,
    log,
    healthMonitor,
  );
  serverSetup.setup();

  // Wire the Tasks 31-36 MCP primitives (resource subscriptions + coalesced
  // notifications, workflow prompts, completions, and the adaptive client
  // profile) onto the server. Runs after setup() so the read-only resource and
  // tools handlers already exist when the fail-closed PrimitiveRegistry validates
  // the advertised surface. Its cleanup helper drains every primitive store and
  // the flush timer on server close.
  const wiredPrimitives = wirePrimitives(server);

  // Forward inbound notifications/cancelled to the automation bridge so the
  // matching queued or inflight Unreal work is cancelled. This is the TS stdio
  // counterpart to the native /mcp transport's cancellation and converges on
  // the same idempotent cancellation primitive as SDK AbortSignal cancellation.
  server.setNotificationHandler(CancelledNotificationSchema, (notification) => {
    const rawId = (notification.params as { requestId?: string | number }).requestId;
    if (rawId === undefined) return;
    const requestId = canonicalizeMcpRequestId(rawId);
    automationBridge.cancelMcpRequest(requestId, 'Client cancelled request');
  });

  return {
    server,
    bridge,
    automationBridge,
    healthMonitor,
    metricsServer,
    wiredPrimitives,
  };
}
