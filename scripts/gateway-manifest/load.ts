// scripts/gateway-manifest/load.ts
// Typed loader for the canonical pilot capability catalog input.
// Reads a JSON file and returns the raw parsed value for downstream
// validation. Fails with typed errors on missing/unreadable/invalid JSON
// and rejects oversized input via a stat-based size cap before readFileSync.
// No swallowed catch, no empty-array fallback.

import { readFileSync } from 'node:fs';
import { fileSizeBytes, PilotPathError } from './path-policy.js';

/** Maximum catalog file size accepted by the loader (50 MiB). */
export const PILOT_CATALOG_MAX_BYTES = 50 * 1024 * 1024;

export type PilotCatalogLoadError =
  | { readonly code: 'PILOT_CATALOG_MISSING'; readonly path: string; readonly message: string }
  | { readonly code: 'PILOT_CATALOG_UNREADABLE'; readonly path: string; readonly message: string }
  | { readonly code: 'PILOT_CATALOG_INVALID_JSON'; readonly path: string; readonly message: string }
  | { readonly code: 'PILOT_CATALOG_TOO_LARGE'; readonly path: string; readonly bytes: number; readonly limit: number; readonly message: string };

export class PilotCatalogError extends Error {
  readonly code: PilotCatalogLoadError['code'];
  readonly catalogPath: string;
  constructor(detail: PilotCatalogLoadError) {
    super(detail.message);
    this.name = 'PilotCatalogError';
    this.code = detail.code;
    this.catalogPath = detail.path;
  }
}

export function defaultPilotCatalogPath(root: string): string {
  const envPath = process.env.MCP_PILOT_CATALOG_PATH;
  return envPath && envPath.length > 0 ? envPath : `${root}/src/tools/catalog/capabilities/catalog.json`;
}

function isErrnoException(error: unknown): error is NodeJS.ErrnoException {
  return error instanceof Error && 'code' in error && typeof error.code === 'string';
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function loadPilotCatalogRaw(path: string): unknown {
  let bytes: number;
  try {
    bytes = fileSizeBytes(path);
  } catch (error) {
    if (error instanceof PilotPathError) throw error;
    if (isErrnoException(error) && (error.code === 'ENOENT' || error.code === 'ENOTDIR')) {
      throw new PilotCatalogError({
        code: 'PILOT_CATALOG_MISSING',
        path,
        message: `Pilot catalog not found at ${path}. Set MCP_PILOT_CATALOG_PATH or create the file.`
      });
    }
    throw new PilotCatalogError({
      code: 'PILOT_CATALOG_UNREADABLE',
      path,
      message: `Pilot catalog at ${path} is unreadable: ${errorMessage(error)}`
    });
  }

  if (bytes > PILOT_CATALOG_MAX_BYTES) {
    throw new PilotCatalogError({
      code: 'PILOT_CATALOG_TOO_LARGE',
      path,
      bytes,
      limit: PILOT_CATALOG_MAX_BYTES,
      message:
        `Pilot catalog at ${path} is ${bytes} bytes, which exceeds the ${PILOT_CATALOG_MAX_BYTES} byte ` +
        '(50 MiB) limit. The canonical capability catalog must stay small; split or trim the file.'
    });
  }

  let text: string;
  try {
    text = readFileSync(path, 'utf8');
  } catch (error) {
    if (isErrnoException(error) && (error.code === 'ENOENT' || error.code === 'ENOTDIR')) {
      throw new PilotCatalogError({
        code: 'PILOT_CATALOG_MISSING',
        path,
        message: `Pilot catalog not found at ${path}. Set MCP_PILOT_CATALOG_PATH or create the file.`
      });
    }
    throw new PilotCatalogError({
      code: 'PILOT_CATALOG_UNREADABLE',
      path,
      message: `Pilot catalog at ${path} is unreadable: ${errorMessage(error)}`
    });
  }
  try {
    return JSON.parse(text);
  } catch (error) {
    throw new PilotCatalogError({
      code: 'PILOT_CATALOG_INVALID_JSON',
      path,
      message: `Pilot catalog at ${path} is not valid JSON: ${errorMessage(error)}`
    });
  }
}
