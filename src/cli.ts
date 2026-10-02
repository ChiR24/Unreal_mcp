#!/usr/bin/env node
import { Logger } from './utils/logging/logger.js';

// `proxy` starts the restart-proof stdio front for the plugin's native HTTP endpoint; anything else starts the
// stdio MCP server. Each path imports only its own modules.
if (process.argv[2] === 'proxy') {
  const { startNativeProxy } = await import('./proxy/native-proxy.js');
  startNativeProxy();
  process.stdin.once('end', () => process.exit(0));
} else {
  const { startStdioServer } = await import('./index.js');
  startStdioServer().catch((err: unknown) => {
    new Logger('CLI').error('Failed to start server:', err instanceof Error ? err : String(err));
    process.exit(1);
  });
}
