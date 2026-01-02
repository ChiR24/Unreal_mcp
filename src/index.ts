
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { Logger } from './utils/logger.js';
import { UnrealBridge } from './unreal-bridge.js';
import { AutomationBridge } from './automation/index.js';
import { createRequire } from 'node:module';
import { responseValidator } from './utils/response-validator.js';
import { z } from 'zod';
import { initializeWASM } from './wasm/index.js';
import { consolidatedToolDefinitions } from './tools/consolidated-tool-definitions.js';
import { HealthMonitor } from './services/health-monitor.js';
import { ServerSetup } from './server-setup.js';
import { startMetricsServer } from './services/metrics-server.js';
import { config } from './config.js';
import { GraphQLServer } from './graphql/server.js';

const require = createRequire(import.meta.url);
const packageInfo: { name?: string; version?: string } = (() => {
  try {
    return require('../package.json');
  } catch (error) {
    const log = new Logger('UE-MCP');
    log.debug('Unable to read package.json for server metadata', error);
    return {};
  }
})();
const DEFAULT_SERVER_NAME = typeof packageInfo.name === 'string' && packageInfo.name.trim().length > 0
  ? packageInfo.name
  : 'unreal-engine-mcp';
const DEFAULT_SERVER_VERSION = typeof packageInfo.version === 'string' && packageInfo.version.trim().length > 0
  ? packageInfo.version
  : '0.5.8';

function routeStdoutLogsToStderr(): void {
  if (!config.MCP_ROUTE_STDOUT_LOGS) {
    return;
  }

  const writeToStderr = (...args: unknown[]): void => {
    const line = args
      .map((a) => (typeof a === 'string' ? a : JSON.stringify(a)))
      .join(' ');
    process.stderr.write(`${line}\n`);
  };

  console.log = (...args: unknown[]): void => { writeToStderr(...args); };
  console.info = (...args: unknown[]): void => { writeToStderr(...args); };
  if (typeof console.debug === 'function') {
    console.debug = (...args: unknown[]): void => { writeToStderr(...args); };
  }
}

const log = new Logger('UE-MCP');

// Ensure stdout remains JSON-only for MCP by routing logs to stderr unless opted out.
routeStdoutLogsToStderr();

// Configuration
const CONFIG = {
  MAX_RETRY_ATTEMPTS: 3,
  RETRY_DELAY_MS: 2000,
  SERVER_NAME: DEFAULT_SERVER_NAME,
  SERVER_VERSION: DEFAULT_SERVER_VERSION,
  AUTOMATION_HEARTBEAT_MS: 15000,
  HEALTH_CHECK_INTERVAL_MS: 30000
};

export function createServer() {
  const bridge = new UnrealBridge();
  const healthMonitor = new HealthMonitor(log);

  const automationBridge = new AutomationBridge({
    serverName: CONFIG.SERVER_NAME,
    serverVersion: CONFIG.SERVER_VERSION,
    heartbeatIntervalMs: CONFIG.AUTOMATION_HEARTBEAT_MS,
    clientMode: config.MCP_AUTOMATION_CLIENT_MODE
  });
  bridge.setAutomationBridge(automationBridge);


  automationBridge.on('connected', ({ metadata, port, protocol }) => {
    log.info(
      `Automation bridge connected (port=${port}, protocol=${protocol ?? 'none'})`,
      metadata
    );
  });

  automationBridge.on('disconnected', ({ code, reason, port, protocol }) => {
    log.info(
      `Automation bridge disconnected (code=${code}, reason=${reason || 'n/a'}, port=${port}, protocol=${protocol ?? 'none'})`
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

  // Optionally expose Prometheus-style metrics via /metrics
  const metricsServer = startMetricsServer({ healthMonitor, automationBridge, logger: log });

  // Initialize GraphQL server (controlled by GRAPHQL_ENABLED env var)
  const graphqlServer = new GraphQLServer(bridge, automationBridge);
  graphqlServer.start().catch((error) => {
    log.warn('GraphQL server failed to start:', error);
  });

  // Initialize WebAssembly module for high-performance operations (5-8x faster)
  log.debug('Initializing WebAssembly integration...');
  initializeWASM().then(() => {
    log.info('✅ WebAssembly integration initialized (JSON parsing and math operations)');
  }).catch((error) => {
    log.warn('⚠️ WebAssembly initialization failed, using TypeScript fallbacks:', error);
  });

  // Initialize response validation with schemas
  log.debug('Initializing response validation...');
  const toolDefs = consolidatedToolDefinitions as Array<{ name: string; outputSchema?: Record<string, unknown> }>;
  toolDefs.forEach((tool) => {
    if (tool.outputSchema) {
      responseValidator.registerSchema(tool.name, tool.outputSchema);
    }
  });
  log.debug(`Registered ${responseValidator.getStats().totalSchemas} output schemas for validation`);

  log.debug('Server starting without connecting to Unreal Engine');
  healthMonitor.metrics.connectionStatus = 'disconnected';

  const server = new Server(
    {
      name: CONFIG.SERVER_NAME,
      version: CONFIG.SERVER_VERSION
    },
    {
      capabilities: {
        tools: {},
        resources: {},
        prompts: {}
      }
    }
  );

  // Setup server using helper class
  const serverSetup = new ServerSetup(server, bridge, automationBridge, log, healthMonitor);
  serverSetup.setup(); // Register tools, resources, and prompts

  return { server, bridge, automationBridge, graphqlServer, metricsServer };
}

// Export configuration schema for session UI and runtime validation
export const configSchema = z.object({
  logLevel: z.enum(['debug', 'info', 'warn', 'error']).optional().default('info').describe('Runtime log level'),
  projectPath: z.string().optional().default('C:/Users/YourName/Documents/Unreal Projects/YourProject').describe('Absolute path to your Unreal .uproject file')
});

// Default export for runtime configuration support.
export default function createServerDefault({ config }: { config?: Record<string, unknown> } = {}) {
  try {
    if (config) {
      if (typeof config.logLevel === 'string') process.env.LOG_LEVEL = config.logLevel;
      if (typeof config.projectPath === 'string' && (config.projectPath as string).trim()) process.env.UE_PROJECT_PATH = config.projectPath as string;
    }
  } catch (e) {
    const errObj = e as Record<string, unknown> | null;
    log.debug('[createServerDefault] Failed to apply config to environment:', errObj?.message ? String(errObj.message) : String(e));
  }

  const { server } = createServer();
  return server;
}

export async function startStdioServer() {
  const { server, bridge, automationBridge, graphqlServer, metricsServer } = createServer();
  const transport = new StdioServerTransport();
  let shuttingDown = false;

  const closeMetricsServer = async (): Promise<void> => {
    if (!metricsServer) {
      return;
    }

    await new Promise<void>((resolve) => {
      try {
        metricsServer.close((error?: Error) => {
          const errorObj = error as Record<string, unknown> | undefined;
          const errorCode = errorObj?.code;
          if (error && errorCode !== 'ERR_SERVER_NOT_RUNNING') {
            log.warn('Failed to close metrics server cleanly', error);
          }
          resolve();
        });
      } catch (error) {
        const errorObj = error as Record<string, unknown> | null;
        const errorCode = errorObj?.code;
        if (errorCode !== 'ERR_SERVER_NOT_RUNNING') {
          log.warn('Failed to close metrics server cleanly', error);
        }
        resolve();
      }
    });
  };

  const handleShutdown = async (signal?: NodeJS.Signals) => {
    if (shuttingDown) {
      return;
    }
    shuttingDown = true;
    const reason = signal ? ` due to ${signal}` : '';
    log.info(`Shutting down MCP server${reason}`);
    try {
      automationBridge.stop();
    } catch (error) {
      log.warn('Failed to stop automation bridge cleanly', error);
    }

    try {
      bridge.dispose();
    } catch (error) {
      log.warn('Failed to dispose Unreal bridge cleanly', error);
    }

    try {
      await closeMetricsServer();
    } catch (error) {
      log.warn('Failed to close metrics server cleanly', error);
    }

    try {
      await graphqlServer.stop();
    } catch (error) {
      log.warn('Failed to stop GraphQL server cleanly', error);
    }

    try {
      const serverObj = server as unknown as Record<string, unknown>;
      if (typeof serverObj.close === 'function') {
        await (serverObj.close as () => Promise<void>)();
      }
    } catch (error) {
      log.warn('Failed to close MCP server transport cleanly', error);
    }

    if (signal) {
      process.exit(0);
    }
  };

  ['SIGINT', 'SIGTERM'].forEach((signal) => {
    process.once(signal as NodeJS.Signals, () => {
      void handleShutdown(signal as NodeJS.Signals);
    });
  });

  process.once('beforeExit', () => {
    try {
      automationBridge.stop();
    } catch { }
    try {
      bridge.dispose();
    } catch { }
    try {
      metricsServer?.close();
    } catch { }
  });
 
  process.once('exit', () => {
    try {
      automationBridge.stop();
    } catch { }
    try {
      bridge.dispose();
    } catch { }
    try {
      metricsServer?.close();
    } catch { }
  });


  const originalWrite = process.stdout.write;
   
  process.stdout.write = function (...args: [string | Uint8Array, ...unknown[]]) {
    const message = args[0];
    if (typeof message === 'string' && message.includes('jsonrpc')) {
      log.debug(`Sending to client: ${message.substring(0, 200)}...`);
    }
    return originalWrite.apply(process.stdout, args as Parameters<typeof originalWrite>);
  } as typeof process.stdout.write;

  await server.connect(transport);
  log.info('Unreal Engine MCP Server started on stdio');
}

// Direct execution is handled via src/cli.ts to keep this module side-effect free.
