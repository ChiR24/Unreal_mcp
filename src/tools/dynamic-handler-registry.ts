import { Logger } from '../utils/logger.js';
import type { ITools } from '../types/tool-interfaces.js';
import fs from 'fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const log = new Logger('DynamicHandlerRegistry');

export interface DynamicHandlerContext {
  name: string;
  action: string;
  args: any;
  tools: ITools;
  defaultHandler: () => Promise<any>;
}

export type DynamicHandlerFn = (ctx: DynamicHandlerContext) => Promise<any>;

interface RawHandlerEntry {
  tool: string;
  module: string;
  function?: string;
  actions?: string[];
}

interface RawRegistry {
  handlers?: RawHandlerEntry[];
}

interface LoadedHandler {
  tool: string;
  actions?: string[];
  fn: DynamicHandlerFn;
}

let registryLoaded = false;
const loadedHandlers: LoadedHandler[] = [];

async function loadRegistryIfNeeded(): Promise<void> {
  if (registryLoaded) return;
  registryLoaded = true;

  // Project root (works for both src/ and dist/ builds)
  const rootUrl = new URL('../..', import.meta.url);

  const envPath = (process.env.MCP_HANDLER_REGISTRY || '').trim();
  let configUrl: URL | null = null;

  try {
    if (envPath) {
      // Allow absolute paths, file:// URLs, or paths relative to project root
      if (envPath.startsWith('file:')) {
        configUrl = new URL(envPath);
      } else if (path.isAbsolute(envPath)) {
        configUrl = pathToFileURL(envPath);
      } else {
        configUrl = new URL(envPath, rootUrl);
      }
    } else {
      // Default: handler-registry.json at project root
      configUrl = new URL('handler-registry.json', rootUrl);
    }

    let jsonText: string;
    try {
      jsonText = await fs.readFile(configUrl, 'utf8');
    } catch (err: any) {
      if (err && (err.code === 'ENOENT' || err.code === 'ERR_MODULE_NOT_FOUND')) {
        log.debug(`Handler registry not found at ${configUrl.href}; dynamic handlers disabled.`);
        return;
      }
      log.warn(`Failed to read handler registry from ${configUrl.href}:`, err);
      return;
    }

    let raw: RawRegistry;
    try {
      raw = JSON.parse(jsonText) as RawRegistry;
    } catch (err) {
      log.warn(`Invalid JSON in handler registry at ${configUrl.href}:`, err);
      return;
    }

    if (!raw.handlers || !Array.isArray(raw.handlers)) {
      log.debug('Handler registry contains no handlers.');
      return;
    }

    for (const entry of raw.handlers) {
      if (!entry || typeof entry.tool !== 'string' || typeof entry.module !== 'string') {
        continue;
      }
      try {
        // Resolve module relative to project root by default
        const modUrl = new URL(entry.module, rootUrl);
        const mod: any = await import(modUrl.href);

        const exportName = entry.function && entry.function.trim().length > 0
          ? entry.function.trim()
          : 'default';

        const fnExport = mod[exportName];
        if (typeof fnExport !== 'function') {
          log.warn(
            `Dynamic handler module ${modUrl.href} for tool '${entry.tool}' does not export a callable '${exportName}'.`
          );
          continue;
        }

        const wrapped: DynamicHandlerFn = fnExport;
        loadedHandlers.push({
          tool: entry.tool,
          actions: Array.isArray(entry.actions) ? entry.actions.slice() : undefined,
          fn: wrapped
        });

        log.info(`Registered dynamic handler for tool '${entry.tool}' from ${modUrl.href}`);
      } catch (err) {
        log.warn(
          `Failed to load dynamic handler for tool '${entry.tool}' from module spec '${entry.module}':`,
          err as any
        );
      }
    }
  } catch (err) {
    log.warn('Unexpected error while loading handler registry:', err as any);
  }
}

/**
 * Resolve a dynamic handler for a given tool/action pair.
 * Returns null when no handler is configured.
 */
export async function getDynamicHandlerForTool(
  toolName: string,
  action?: string
): Promise<DynamicHandlerFn | null> {
  await loadRegistryIfNeeded();
  if (!loadedHandlers.length) return null;

  const name = toolName;
  const act = action;

  for (const handler of loadedHandlers) {
    if (handler.tool !== name) continue;
    if (handler.actions && act && !handler.actions.includes(act)) continue;
    return handler.fn;
  }

  return null;
}
