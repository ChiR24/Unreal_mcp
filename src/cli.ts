#!/usr/bin/env node
import { startStdioServer } from './index.js';
import { Logger } from './utils/logging/logger.js';

startStdioServer().catch((err: unknown) => {
  new Logger('CLI').error('Failed to start server:', err instanceof Error ? err : String(err));
  process.exit(1);
});
