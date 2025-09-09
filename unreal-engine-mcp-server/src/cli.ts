#!/usr/bin/env node
import { startStdioServer } from './index.js';

startStdioServer().catch((err) => {
  console.error('Failed to start server:', err);
  process.exit(1);
});
