import { serveStdio } from '@modelcontextprotocol/server/stdio';

import {
  createServer,
  log,
} from './server-factory.js';

export async function startStdioServer(): Promise<void> {
  const {
    server,
    buildServer,
    automationBridge,
    healthMonitor,
  } = createServer();
  // The opening exchange picks the era: initialize serves 2025-era clients as before, a request whose _meta names
  // 2026-07-28 is served per request. The first instance is the server built above; a second one is only asked for
  // when a discover probe is followed by an initialize.
  let served = false;
  const handle = serveStdio(() => {
    if (served) return buildServer();
    served = true;
    return server;
  }, { onerror: (error) => log.warn('Stdio serving error', error) });
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
      await handle.close();
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

  log.info('Unreal Engine MCP Server started on stdio');
}
