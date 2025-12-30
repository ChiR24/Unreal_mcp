#!/usr/bin/env node
// Dynamic import loader: prefer compiled JS when present (./index.js) but
// gracefully fall back to TypeScript source (./index.ts) when running via
// ts-node-esm or similar dev workflows where compiled JS isn't available.

import { Logger } from './utils/logger.js';

const log = new Logger('CLI');

(async () => {
  try {
    const m = await import('./index.js');
    if (m && typeof m.startStdioServer === 'function') {
      await m.startStdioServer();
    } else {
      throw new Error('startStdioServer not exported from index.js');
    }
  } catch (err) {
    // If index.js cannot be resolved, try importing the TypeScript source
    // at runtime (useful when running via ts-node-esm). Cast the error when
    // inspecting runtime-only properties like `code`.
    const errObj = err as Record<string, unknown> | null;
    if (err && ((errObj?.code === 'ERR_MODULE_NOT_FOUND') || String(err).includes('Unable to resolve'))) {
      try {
          const tsModuleSpecifier = new URL('./index.ts', import.meta.url).href;
          const m2 = await import(tsModuleSpecifier);
          if (m2 && typeof m2.startStdioServer === 'function') {
            await m2.startStdioServer();
          } else {
            throw new Error('startStdioServer not exported from index.ts');
          }
      } catch (err2) {
        log.error('Failed to start server (fallback to TypeScript failed):', err2);
        process.exit(1);
      }
    }
    log.error('Failed to start server:', err);
    process.exit(1);
  }
})();
