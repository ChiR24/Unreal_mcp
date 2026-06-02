#!/usr/bin/env node
// Dynamic import loader: prefer compiled JS when present (./index.js) but
// gracefully fall back to TypeScript source (./index.ts) when running via
// ts-node-esm or similar dev workflows where compiled JS isn't available.

import { Logger } from './utils/logger.js';
import { isRecord } from './utils/type-guards.js';

const log = new Logger('CLI');

type ServerModule = {
  startStdioServer?: unknown;
};

async function startFromModule(module: ServerModule, source: string): Promise<void> {
  if (typeof module.startStdioServer !== 'function') {
    throw new Error(`startStdioServer not exported from ${source}`);
  }

  await module.startStdioServer();
}

function isResolvableSourceError(error: unknown): boolean {
  return (isRecord(error) && error.code === 'ERR_MODULE_NOT_FOUND') || String(error).includes('Unable to resolve');
}

(async () => {
  try {
    await startFromModule(await import('./index.js'), 'index.js');
  } catch (err) {
    if (isResolvableSourceError(err)) {
      try {
        const tsModuleSpecifier = new URL('./index.ts', import.meta.url).href;
        await startFromModule(await import(tsModuleSpecifier), 'index.ts');
        return;
      } catch (err2) {
        log.error('Failed to start server (fallback to TypeScript failed):', err2);
        process.exit(1);
      }
    }
    log.error('Failed to start server:', err);
    process.exit(1);
  }
})();
