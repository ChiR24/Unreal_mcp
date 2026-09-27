import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CancelledNotificationSchema } from '@modelcontextprotocol/sdk/types.js';

import { AutomationBridge } from '../automation/index.js';
import { AutomationLogger } from '../automation/log-redaction.js';
import { PACKAGE } from '../constants.js';
import { ServerSetup } from '../server-setup.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { UNREAL_GATEWAY_INSTRUCTIONS } from '../tools/catalog/unreal-gateway-definition.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { canonicalizeMcpRequestId } from '../automation/request-context.js';
import { SERVER_CAPABILITIES, wirePrimitives } from './mcp-primitives/primitive-wiring.js';

const { name: SERVER_NAME, version: SERVER_VERSION } = PACKAGE;
const AUTOMATION_HEARTBEAT_MS = 15_000;

export const log = new AutomationLogger('UE-MCP');

export function routeStdoutLogsToStderr(): void {
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
  console.debug = writeToStderr;
}

export function createServer() {
  const bridge = new UnrealBridge();
  const healthMonitor = new HealthMonitor(log);
  const automationBridge = new AutomationBridge({
    serverName: SERVER_NAME,
    serverVersion: SERVER_VERSION,
    heartbeatIntervalMs: AUTOMATION_HEARTBEAT_MS,
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

  log.debug('Server starting without connecting to Unreal Engine');
  healthMonitor.metrics.connectionStatus = 'disconnected';

  const server = new Server(
    {
      name: SERVER_NAME,
      version: SERVER_VERSION,
    },
    {
      capabilities: SERVER_CAPABILITIES,
      // Injected into the client's system prompt by most MCP hosts: the one place a
      // model reads the search -> describe -> execute procedure before its first call.
      instructions: UNREAL_GATEWAY_INSTRUCTIONS,
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

  wirePrimitives(server);

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
  };
}
