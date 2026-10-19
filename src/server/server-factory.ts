import { Server } from '@modelcontextprotocol/server';

import { AutomationBridge } from '../automation/index.js';
import { AutomationLogger } from '../automation/log-redaction.js';
import { PACKAGE } from '../constants.js';
import { ServerSetup } from '../server-setup.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { UNREAL_GATEWAY_INSTRUCTIONS } from '../tools/catalog/unreal-gateway-definition.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { CACHE_HINTS, SERVER_CAPABILITIES, wirePrimitives } from './mcp-primitives/primitive-wiring.js';
import { consentStateCodec } from './gateway/gateway-execute-consent.js';

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

  // One Server per era instance: stdio serves one, but a 2026-07-28 discover probe followed by an initialize
  // (a client falling back) makes serveStdio build a second. Bridge, services and listeners are shared.
  const buildServer = (): Server => {
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
        cacheHints: CACHE_HINTS,
        // Consent asked through a 2026-07-28 client: its requestState is verified before the handler runs.
        requestState: { verify: consentStateCodec.verify },
        // Only 2026-07-28 requests are asked; an older client keeps describe's consentGrant, never a pushed prompt.
        inputRequired: { legacyShim: false },
      },
    );

    const forwardAutomationEvent = (event: Record<string, unknown>): void => {
      server.notification({
        method: 'notifications/unreal/automation_event',
        params: event,
      }).catch((error: unknown) => {
        log.error(
          'Failed to forward Unreal automation event notification',
          error instanceof Error ? error : String(error),
        );
      });
    };
    automationBridge.on('automationEvent', forwardAutomationEvent);

    const serverSetup = new ServerSetup(
      server,
      bridge,
      automationBridge,
      log,
      healthMonitor,
    );
    serverSetup.setup();

    wirePrimitives(server);

    // notifications/cancelled stays the SDK's: it aborts the request's signal, which the tool registry turns into
    // automationBridge.cancelMcpRequest, and the SDK then sends no late response. A handler set here replaced the
    // SDK's, so the signal never aborted and the cancelled call still answered.

    // Chained, as the tool registry chains its own drain.
    const previousOnClose = server.onclose;
    server.onclose = (): void => {
      automationBridge.off('automationEvent', forwardAutomationEvent);
      previousOnClose?.();
    };
    return server;
  };

  return {
    server: buildServer(),
    buildServer,
    bridge,
    automationBridge,
    healthMonitor,
  };
}
