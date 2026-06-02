import fs from 'node:fs/promises';
import path from 'node:path';
import { DEFAULT_SKYLIGHT_INTENSITY, DEFAULT_SUN_INTENSITY, DEFAULT_TIME_OF_DAY } from '../constants.js';
import type { StandardActionResponse } from '../types/tool-interfaces.js';

function isPathWithinDirectory(candidate: string, directory: string): boolean {
  const relative = path.relative(directory, candidate);
  return relative === '' || (relative.length > 0 && !relative.startsWith('..') && !path.isAbsolute(relative));
}

function getErrorCode(error: unknown): string | undefined {
  if (typeof error !== 'object' || error === null || !('code' in error)) {
    return undefined;
  }
  const code = (error as { code?: unknown }).code;
  return typeof code === 'string' ? code : undefined;
}

function isJsonObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function parseSnapshotObject(contents: string): Record<string, unknown> | undefined {
  try {
    const parsed: unknown = JSON.parse(contents);
    return isJsonObject(parsed) ? parsed : undefined;
  } catch {
    return undefined;
  }
}

async function validateSnapshotFilesystemTarget(targetPath: string): Promise<{ success: false; error: string } | { success: true }> {
  const cwd = path.resolve(process.cwd());
  let realCwd: string;
  try {
    realCwd = await fs.realpath(cwd);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { success: false, error: `Failed to inspect project directory: ${message}` };
  }

  const absoluteTarget = path.resolve(targetPath);
  const relativeTarget = path.relative(cwd, absoluteTarget);
  if (relativeTarget === '' || relativeTarget.startsWith('..') || path.isAbsolute(relativeTarget)) {
    return { success: false, error: 'SECURITY_VIOLATION: Snapshot path must be within the project directory' };
  }

  let current = cwd;
  const segments = relativeTarget.split(path.sep).filter(segment => segment.length > 0);
  for (const segment of segments) {
    current = path.join(current, segment);

    try {
      const currentStat = await fs.lstat(current);
      if (currentStat.isSymbolicLink()) {
        return { success: false, error: 'SECURITY_VIOLATION: Snapshot path cannot contain symbolic link components' };
      }

      const realCurrent = await fs.realpath(current);
      if (!isPathWithinDirectory(realCurrent, realCwd)) {
        return { success: false, error: 'SECURITY_VIOLATION: Snapshot path must resolve within the project directory' };
      }
    } catch (error) {
      const code = getErrorCode(error);
      if (code === 'ENOENT' || code === 'ENOTDIR') {
        return { success: true };
      }

      const message = error instanceof Error ? error.message : String(error);
      return { success: false, error: `Failed to inspect snapshot path: ${message}` };
    }
  }

  return { success: true };
}

function validateSnapshotFilename(filename: string): { success: false; error: string } | { success: true } {
  if (filename.includes('\0')) {
    return { success: false, error: 'SECURITY_VIOLATION: Filename cannot contain null bytes' };
  }

  if (filename.includes('..') || filename.includes('/') || filename.includes('\\')) {
    return { success: false, error: 'SECURITY_VIOLATION: Filename cannot contain path separators or traversal' };
  }

  if (filename.includes(':')) {
    return { success: false, error: 'SECURITY_VIOLATION: Filename cannot contain drive or stream separators' };
  }

  return { success: true };
}

export function validateSnapshotPath(inputPath: unknown): { isValid: false; error: string } | { isValid: true; safePath: string } {
  if (!inputPath || typeof inputPath !== 'string') {
    return { isValid: false, error: 'Path is required' };
  }

  const trimmed = inputPath.trim();
  if (trimmed.length === 0) {
    return { isValid: false, error: 'Path cannot be empty' };
  }

  if (trimmed.includes('\0')) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Path cannot contain null bytes' };
  }

  if (/^[a-zA-Z]:[/\\]/.test(trimmed) || trimmed.includes(':')) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Absolute paths with drive letters are not allowed' };
  }

  const slashNormalized = trimmed.replace(/\\/g, '/');
  const rawSegments = slashNormalized.split('/').filter(segment => segment.length > 0);
  if (rawSegments.includes('..')) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Path traversal (..) is not allowed' };
  }

  const cwd = process.cwd();
  if (slashNormalized.startsWith('/')) {
    const systemPathPrefixes = ['/etc/', '/var/', '/usr/', '/bin/', '/sbin/', '/root/', '/home/', '/opt/', '/proc/', '/sys/', '/dev/', '/tmp/'];
    for (const prefix of systemPathPrefixes) {
      if (slashNormalized.toLowerCase().startsWith(prefix.toLowerCase())) {
        return { isValid: false, error: `SECURITY_VIOLATION: System directory paths (${prefix}) are not allowed` };
      }
    }

    const exactSystemPaths = ['/etc', '/var', '/usr', '/bin', '/sbin', '/root', '/home', '/opt', '/proc', '/sys', '/dev', '/tmp'];
    if (exactSystemPaths.some(sp => slashNormalized.toLowerCase() === sp.toLowerCase())) {
      return { isValid: false, error: 'SECURITY_VIOLATION: System directory paths are not allowed' };
    }

    const allowedUePaths = ['/Temp/', '/Saved/', '/Game/'];
    const isAllowedUePath = allowedUePaths.some(uePath =>
      slashNormalized.toLowerCase().startsWith(uePath.toLowerCase()) ||
      slashNormalized.toLowerCase() === uePath.slice(0, -1).toLowerCase()
    );

    if (!isAllowedUePath) {
      return { isValid: false, error: 'SECURITY_VIOLATION: Only UE project-relative paths (/Temp, /Saved, /Game) are allowed' };
    }

    let mappedPath: string;
    if (slashNormalized.toLowerCase().startsWith('/temp')) {
      mappedPath = path.join(cwd, 'temp', slashNormalized.slice(6));
    } else if (slashNormalized.toLowerCase().startsWith('/saved')) {
      mappedPath = path.join(cwd, 'Saved', slashNormalized.slice(7));
    } else if (slashNormalized.toLowerCase().startsWith('/game')) {
      mappedPath = path.join(cwd, 'Content', slashNormalized.slice(6));
    } else {
      return { isValid: false, error: 'SECURITY_VIOLATION: Unrecognized UE path' };
    }

    const finalPath = path.normalize(mappedPath);
    const cwdWithSep = cwd.endsWith(path.sep) ? cwd : cwd + path.sep;
    if (finalPath !== cwd && !finalPath.startsWith(cwdWithSep)) {
      return { isValid: false, error: 'SECURITY_VIOLATION: Path must be within the project directory' };
    }

    return { isValid: true, safePath: finalPath };
  }

  const resolvedPath = path.resolve(cwd, path.normalize(slashNormalized));
  const cwdWithSep = cwd.endsWith(path.sep) ? cwd : cwd + path.sep;
  if (resolvedPath !== cwd && !resolvedPath.startsWith(cwdWithSep)) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Path must be within the project directory' };
  }

  return { isValid: true, safePath: resolvedPath };
}

function envNumber(name: string, fallback: number, allowNegative = true): number {
  const raw = process.env[name];
  if (raw !== undefined) {
    const parsed = Number(raw);
    if (Number.isFinite(parsed) && (allowNegative || parsed >= 0)) {
      return parsed;
    }
  }
  return fallback;
}

function getSnapshotTarget(params: { path?: unknown; filename?: unknown }): { success: false; error: string } | { success: true; targetPath: string } {
  const rawPath = typeof params?.path === 'string' && params.path.trim().length > 0
    ? params.path.trim()
    : './tmp/unreal-mcp/env_snapshot.json';
  const rawFilename = typeof params?.filename === 'string' && params.filename.trim().length > 0
    ? params.filename.trim()
    : undefined;

  const basePathValidation = validateSnapshotPath(rawPath);
  if (!basePathValidation.isValid) {
    return { success: false, error: basePathValidation.error };
  }

  if (rawFilename) {
    const filenameValidation = validateSnapshotFilename(rawFilename);
    if (!filenameValidation.success) {
      return filenameValidation;
    }
    return { success: true, targetPath: path.join(basePathValidation.safePath, rawFilename) };
  }

  const hasExt = /\.[a-z0-9]+$/i.test(rawPath);
  return {
    success: true,
    targetPath: hasExt ? basePathValidation.safePath : path.join(basePathValidation.safePath, 'env_snapshot.json')
  };
}

export async function exportEnvironmentSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse> {
  try {
    const target = getSnapshotTarget(params);
    if (!target.success) {
      return target;
    }

    const filesystemTarget = await validateSnapshotFilesystemTarget(target.targetPath);
    if (!filesystemTarget.success) {
      return filesystemTarget;
    }

    await fs.mkdir(path.dirname(target.targetPath), { recursive: true });
    const snapshot = {
      generatedAt: new Date().toISOString(),
      timeOfDay: envNumber('MCP_ENV_DEFAULT_TIME_OF_DAY', DEFAULT_TIME_OF_DAY),
      sunIntensity: envNumber('MCP_ENV_DEFAULT_SUN_INTENSITY', DEFAULT_SUN_INTENSITY, false),
      skylightIntensity: envNumber('MCP_ENV_DEFAULT_SKYLIGHT_INTENSITY', DEFAULT_SKYLIGHT_INTENSITY, false)
    };
    await fs.writeFile(target.targetPath, JSON.stringify(snapshot, null, 2), 'utf8');

    return {
      success: true,
      message: `Environment snapshot exported to ${target.targetPath}`,
      details: { path: target.targetPath }
    };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { success: false, error: `Failed to export environment snapshot: ${message}` };
  }
}

export async function importEnvironmentSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse> {
  const target = getSnapshotTarget(params);
  if (!target.success) {
    return target;
  }

  try {
    const filesystemTarget = await validateSnapshotFilesystemTarget(target.targetPath);
    if (!filesystemTarget.success) {
      return filesystemTarget;
    }

    let parsed: Record<string, unknown> | undefined = undefined;
    try {
      const contents = await fs.readFile(target.targetPath, 'utf8');
      parsed = parseSnapshotObject(contents);
    } catch (error: unknown) {
      const errObj = error as Record<string, unknown> | null;
      if (errObj && (errObj.code === 'ENOENT' || errObj.code === 'ENOTDIR')) {
        return {
          success: true,
          message: `Environment snapshot file not found at ${target.targetPath}; import treated as no-op`
        };
      }
      throw error;
    }

    return {
      success: true,
      message: `Environment snapshot import handled from ${target.targetPath}`,
      details: parsed
    };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { success: false, error: `Failed to import environment snapshot: ${message}` };
  }
}
