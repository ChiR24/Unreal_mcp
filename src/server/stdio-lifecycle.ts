import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';

import {
  createServer,
  log,
} from './server-factory.js';

type MaybeClosableServer = {
  close?: () => void | Promise<void>;
};

function getErrorCode(error: unknown): unknown {
  if (error && typeof error === 'object' && 'code' in error) {
    return (error as { code?: unknown }).code;
  }
  return undefined;
}

export async function startStdioServer(): Promise<void> {
  const {
    server,
    bridge,
    automationBridge,
    healthMonitor,
    metricsServer,
    wiredPrimitives,
  } = createServer();
  const transport = new StdioServerTransport();
  let shuttingDown = false;

  const closeMetricsServer = async (): Promise<void> => {
    if (!metricsServer) {
      return;
    }

    await new Promise<void>((resolve) => {
      try {
        metricsServer.close((error?: Error) => {
          const errorCode = getErrorCode(error);
          if (error && errorCode !== 'ERR_SERVER_NOT_RUNNING') {
            log.warn('Failed to close metrics server cleanly', error);
          }
          resolve();
        });
      } catch (error) {
        const errorCode = getErrorCode(error);
        if (errorCode !== 'ERR_SERVER_NOT_RUNNING') {
          log.warn(
            'Failed to close metrics server cleanly',
            error instanceof Error ? error : String(error),
          );
        }
        resolve();
      }
    });
  };

  const handleShutdown = async (signal?: NodeJS.Signals): Promise<void> => {
    if (shuttingDown) {
      return;
    }
    shuttingDown = true;
    const reason = signal ? ` due to ${signal}` : '';
    log.info(`Shutting down MCP server${reason}`);

    try {
      healthMonitor.stopHealthChecks();
    } catch (error) {
      log.warn(
        'Failed to stop health checks cleanly',
        error instanceof Error ? error : String(error),
      );
    }
    try {
      wiredPrimitives.dispose();
    } catch (error) {
      log.warn(
        'Failed to drain MCP primitive stores cleanly',
        error instanceof Error ? error : String(error),
      );
    }
    try {
      bridge.dispose();
    } catch (error) {
      log.warn(
        'Failed to dispose Unreal bridge cleanly',
        error instanceof Error ? error : String(error),
      );
    }
    try {
      automationBridge.stop();
    } catch (error) {
      log.warn(
        'Failed to stop automation bridge cleanly',
        error instanceof Error ? error : String(error),
      );
    }
    try {
      await closeMetricsServer();
    } catch (error) {
      log.warn(
        'Failed to close metrics server cleanly',
        error instanceof Error ? error : String(error),
      );
    }
    try {
      const closeServer = (server as MaybeClosableServer).close;
      if (typeof closeServer === 'function') {
        await closeServer.call(server);
      }
    } catch (error) {
      log.warn(
        'Failed to close MCP server transport cleanly',
        error instanceof Error ? error : String(error),
      );
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

  const runLifecycleCleanup = (eventName: 'beforeExit' | 'exit'): void => {
    const runCleanup = (operation: string, cleanup: () => void): void => {
      try {
        cleanup();
      } catch (error) {
        log.debug(
          `Failed to ${operation} during ${eventName}`,
          error instanceof Error ? error : String(error),
        );
      }
    };

    runCleanup('stop health checks', () => healthMonitor.stopHealthChecks());
    runCleanup('drain MCP primitive stores', () => wiredPrimitives.dispose());
    runCleanup('dispose Unreal bridge', () => bridge.dispose());
    runCleanup('stop automation bridge', () => automationBridge.stop());
    runCleanup('close metrics server', () => {
      metricsServer?.close();
    });
  };

  process.once('beforeExit', () => {
    runLifecycleCleanup('beforeExit');
  });
  process.once('exit', () => {
    runLifecycleCleanup('exit');
  });

  const originalWrite = process.stdout.write;
  process.stdout.write = function (
    ...args: [string | Uint8Array, ...unknown[]]
  ) {
    const message = args[0];
    if (typeof message === 'string' && message.includes('jsonrpc')) {
      log.debug(`Sending to client: ${message.substring(0, 200)}...`);
    }
    return originalWrite.apply(
      process.stdout,
      args as Parameters<typeof originalWrite>,
    );
  } as typeof process.stdout.write;

  await server.connect(transport);
  log.info('Unreal Engine MCP Server started on stdio');
}
