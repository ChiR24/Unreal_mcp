import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';

import {
  createServer,
  log,
} from './server-factory.js';

export async function startStdioServer(): Promise<void> {
  const {
    server,
    automationBridge,
    healthMonitor,
  } = createServer();
  const transport = new StdioServerTransport();
  let shuttingDown = false;

  // Idempotent: shutdown, beforeExit and exit may all run it.
  const stopServices = (): void => {
    for (const [what, stop] of [
      ['stop health checks', () => healthMonitor.stopHealthChecks()],
      ['stop automation bridge', () => automationBridge.stop()],
    ] as const) {
      try {
        stop();
      } catch (error) {
        log.warn(`Failed to ${what} cleanly`, error instanceof Error ? error : String(error));
      }
    }
  };

  const handleShutdown = async (signal?: NodeJS.Signals): Promise<void> => {
    if (shuttingDown) {
      return;
    }
    shuttingDown = true;
    log.info(`Shutting down MCP server${signal ? ` due to ${signal}` : ''}`);
    stopServices();
    try {
      await server.close();
    } catch (error) {
      log.warn('Failed to close MCP server transport cleanly', error instanceof Error ? error : String(error));
    }
    if (signal) {
      process.exit(0);
    }
  };

  for (const signal of ['SIGINT', 'SIGTERM'] as const) {
    process.once(signal, () => {
      void handleShutdown(signal);
    });
  }

  process.stdin.once('end', () => {
    void handleShutdown();
  });
  process.stdin.once('close', () => {
    void handleShutdown();
  });
  process.stdin.once('error', (error) => {
    log.warn('Stdio input closed with an error', error);
    void handleShutdown();
  });
  process.once('beforeExit', stopServices);
  process.once('exit', stopServices);

  await server.connect(transport);
  log.info('Unreal Engine MCP Server started on stdio');
}
