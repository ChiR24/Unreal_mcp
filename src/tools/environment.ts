import fs from 'node:fs/promises';
import path from 'node:path';
import { AutomationBridge } from '../automation/index.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { DEFAULT_SKYLIGHT_INTENSITY, DEFAULT_SUN_INTENSITY, DEFAULT_TIME_OF_DAY } from '../constants.js';
import { IEnvironmentTools, StandardActionResponse } from '../types/tool-interfaces.js';

/**
 * Validates a file path to prevent path traversal and arbitrary file access.
 * Security checks:
 * - Rejects Windows absolute paths (drive letters)
 * - Rejects Unix absolute paths (starting with /) except UE-style project-relative paths
 * - Rejects path traversal attempts (..)
 * - Maps UE-style paths (/Temp, /Saved, /Game) to safe project directories
 * - Ensures final path is contained within project directory
 * 
 * @param inputPath - The path to validate
 * @returns Object with isValid flag and optional sanitized path or error
 */
export function validateSnapshotPath(inputPath: string): { isValid: false; error: string } | { isValid: true; safePath: string } {
  if (!inputPath || typeof inputPath !== 'string') {
    return { isValid: false, error: 'Path is required' };
  }

  const trimmed = inputPath.trim();
  if (trimmed.length === 0) {
    return { isValid: false, error: 'Path cannot be empty' };
  }

  // SECURITY: Reject Windows absolute paths (contain drive letter with colon)
  // Matches patterns like C:\, D:/, X:\etc\passwd, etc.
  if (/^[a-zA-Z]:[/\\]/.test(trimmed) || trimmed.includes(':')) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Absolute paths with drive letters are not allowed' };
  }

  // SECURITY: Reject path traversal attempts
  // Normalize the path to resolve any ./ or ../ before checking
  const normalized = path.normalize(trimmed);
  if (normalized.includes('..') || trimmed.includes('..')) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Path traversal (..) is not allowed' };
  }

  const cwd = process.cwd();

  // SECURITY: Handle UE-style project-relative paths (start with /)
  // Only allow specific UE directories that are safe
  if (trimmed.startsWith('/')) {
    // Reject system paths explicitly
    // Note: We check for specific system paths but allow UE's /Temp, /Saved, /Game
    // /tmp/ (lowercase) is a Unix system directory, but /Temp/ (capital T) is UE
    const systemPathPrefixes = ['/etc/', '/var/', '/usr/', '/bin/', '/sbin/', '/root/', '/home/', '/opt/', '/proc/', '/sys/', '/dev/', '/tmp/'];
    for (const prefix of systemPathPrefixes) {
      if (trimmed.toLowerCase().startsWith(prefix.toLowerCase())) {
        return { isValid: false, error: `SECURITY_VIOLATION: System directory paths (${prefix}) are not allowed` };
      }
    }
    
    // Reject exact system path matches (no trailing slash)
    const exactSystemPaths = ['/etc', '/var', '/usr', '/bin', '/sbin', '/root', '/home', '/opt', '/proc', '/sys', '/dev', '/tmp'];
    if (exactSystemPaths.some(sp => trimmed.toLowerCase() === sp.toLowerCase())) {
      return { isValid: false, error: 'SECURITY_VIOLATION: System directory paths are not allowed' };
    }

    // Only allow UE project-relative paths: /Temp/, /Saved/, /Game/
    // These will be mapped to safe directories within the project
    const allowedUePaths = ['/Temp/', '/Saved/', '/Game/'];
    const isAllowedUePath = allowedUePaths.some(uePath => 
      trimmed.toLowerCase().startsWith(uePath.toLowerCase()) || 
      trimmed.toLowerCase() === uePath.slice(0, -1).toLowerCase()
    );

    if (!isAllowedUePath) {
      return { isValid: false, error: 'SECURITY_VIOLATION: Only UE project-relative paths (/Temp, /Saved, /Game) are allowed' };
    }

    // Map UE-style paths to safe project directories
    // /Temp -> project/temp, /Saved -> project/Saved, /Game -> project/Content
    let mappedPath: string;
    if (trimmed.toLowerCase().startsWith('/temp')) {
      // Map /Temp to a temp directory within the project
      mappedPath = path.join(cwd, 'temp', trimmed.slice(6)); // Remove '/Temp/'
    } else if (trimmed.toLowerCase().startsWith('/saved')) {
      mappedPath = path.join(cwd, 'Saved', trimmed.slice(7)); // Remove '/Saved/'
    } else if (trimmed.toLowerCase().startsWith('/game')) {
      mappedPath = path.join(cwd, 'Content', trimmed.slice(6)); // Remove '/Game/'
    } else {
      // This shouldn't happen due to the isAllowedUePath check, but be safe
      return { isValid: false, error: 'SECURITY_VIOLATION: Unrecognized UE path' };
    }

    // Normalize and verify the final path is within the project
    const finalPath = path.normalize(mappedPath);
    if (!finalPath.startsWith(cwd)) {
      return { isValid: false, error: 'SECURITY_VIOLATION: Path must be within the project directory' };
    }

    return { isValid: true, safePath: finalPath };
  }

  // For relative paths, resolve against project directory
  const resolvedPath = path.resolve(cwd, trimmed);
  
  // Ensure the resolved path is within the project directory
  if (!resolvedPath.startsWith(cwd)) {
    return { isValid: false, error: 'SECURITY_VIOLATION: Path must be within the project directory' };
  }

  return { isValid: true, safePath: resolvedPath };
}

export class EnvironmentTools implements IEnvironmentTools {
  constructor(_bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private ensureAutomationBridge(): AutomationBridge {
    if (!this.automationBridge) {
      throw new Error('AUTOMATION_BRIDGE_UNAVAILABLE');
    }
    return this.automationBridge;
  }

  private getDefaultTimeOfDay(): number {
    const raw = process.env.MCP_ENV_DEFAULT_TIME_OF_DAY;
    if (raw !== undefined) {
      const parsed = Number(raw);
      if (Number.isFinite(parsed)) {
        return parsed;
      }
    }
    return DEFAULT_TIME_OF_DAY;
  }

  private getDefaultSunIntensity(): number {
    const raw = process.env.MCP_ENV_DEFAULT_SUN_INTENSITY;
    if (raw !== undefined) {
      const parsed = Number(raw);
      if (Number.isFinite(parsed) && parsed >= 0) {
        return parsed;
      }
    }
    return DEFAULT_SUN_INTENSITY;
  }

  private getDefaultSkylightIntensity(): number {
    const raw = process.env.MCP_ENV_DEFAULT_SKYLIGHT_INTENSITY;
    if (raw !== undefined) {
      const parsed = Number(raw);
      if (Number.isFinite(parsed) && parsed >= 0) {
        return parsed;
      }
    }
    return DEFAULT_SKYLIGHT_INTENSITY;
  }

  private normalizeNumber(value: unknown, field: string, fallback: number): number {
    if (value === undefined || value === null) {
      return fallback;
    }
    const num = typeof value === 'number' ? value : Number(value);
    if (!Number.isFinite(num)) {
      throw new Error(`Invalid ${field}: expected finite number`);
    }
    return num;
  }

  private async invoke(action: string, payload: Record<string, unknown>): Promise<StandardActionResponse> {
    try {
      const bridge = this.ensureAutomationBridge();
      const response = await bridge.sendAutomationRequest(action, payload, { timeoutMs: 40000 });
      if (response.success === false) {
        return {
          success: false,
          error: typeof response.error === 'string' && response.error.length > 0 ? response.error : 'ENVIRONMENT_CONTROL_FAILED',
          message: typeof response.message === 'string' ? response.message : undefined,
          details: typeof response.result === 'object' && response.result ? response.result as Record<string, unknown> : undefined
        } as StandardActionResponse;
      }
      const resultPayload = typeof response.result === 'object' && response.result ? response.result as Record<string, unknown> : undefined;
      return {
        success: true,
        message: typeof response.message === 'string' ? response.message : 'Environment control action succeeded',
        details: resultPayload
      } as StandardActionResponse;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: message || 'ENVIRONMENT_CONTROL_FAILED'
      } as StandardActionResponse;
    }
  }

  async setTimeOfDay(hour: unknown): Promise<StandardActionResponse> {
    const normalizedHour = this.normalizeNumber(hour, 'hour', this.getDefaultTimeOfDay());
    const clampedHour = Math.min(Math.max(normalizedHour, 0), 24);
    return this.invoke('set_time_of_day', { hour: clampedHour });
  }

  async setSunIntensity(intensity: unknown): Promise<StandardActionResponse> {
    const normalized = this.normalizeNumber(intensity, 'intensity', this.getDefaultSunIntensity());
    const finalValue = Math.max(normalized, 0);
    return this.invoke('set_sun_intensity', { intensity: finalValue });
  }

  async setSkylightIntensity(intensity: unknown): Promise<StandardActionResponse> {
    const normalized = this.normalizeNumber(intensity, 'intensity', this.getDefaultSkylightIntensity());
    const finalValue = Math.max(normalized, 0);
    return this.invoke('set_skylight_intensity', { intensity: finalValue });
  }

  async exportSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse> {
    try {
      const rawPath = typeof params?.path === 'string' && params.path.trim().length > 0
        ? params.path.trim()
        : './tmp/unreal-mcp/env_snapshot.json';
      const rawFilename = typeof params?.filename === 'string' && params.filename.trim().length > 0
        ? params.filename.trim()
        : undefined;

      // SECURITY: Validate the path to prevent path traversal attacks
      const basePathValidation = validateSnapshotPath(rawPath);
      if (!basePathValidation.isValid) {
        return {
          success: false,
          error: basePathValidation.error
        } as StandardActionResponse;
      }

      let targetPath: string;
      if (rawFilename) {
        // Validate filename doesn't contain path traversal
        if (rawFilename.includes('..') || rawFilename.includes('/') || rawFilename.includes('\\')) {
          return {
            success: false,
            error: 'SECURITY_VIOLATION: Filename cannot contain path separators or traversal'
          } as StandardActionResponse;
        }
        targetPath = path.join(basePathValidation.safePath, rawFilename);
      } else {
        const hasExt = /\.[a-z0-9]+$/i.test(rawPath);
        if (hasExt) {
          targetPath = basePathValidation.safePath;
        } else {
          const filename = 'env_snapshot.json';
          targetPath = path.join(basePathValidation.safePath, filename);
        }
      }

      // SECURITY: Final validation of the complete target path
      const finalValidation = validateSnapshotPath(targetPath);
      if (!finalValidation.isValid) {
        return {
          success: false,
          error: finalValidation.error
        } as StandardActionResponse;
      }

      await fs.mkdir(path.dirname(targetPath), { recursive: true });
      const snapshot = {
        generatedAt: new Date().toISOString(),
        timeOfDay: this.getDefaultTimeOfDay(),
        sunIntensity: this.getDefaultSunIntensity(),
        skylightIntensity: this.getDefaultSkylightIntensity()
      };
      await fs.writeFile(targetPath, JSON.stringify(snapshot, null, 2), 'utf8');

      return {
        success: true,
        message: `Environment snapshot exported to ${targetPath}`,
        details: { path: targetPath }
      } as StandardActionResponse;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to export environment snapshot: ${message}`
      } as StandardActionResponse;
    }
  }

  async importSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse> {
    const rawPath = typeof params?.path === 'string' && params.path.trim().length > 0
      ? params.path.trim()
      : './tmp/unreal-mcp/env_snapshot.json';
    const rawFilename = typeof params?.filename === 'string' && params.filename.trim().length > 0
      ? params.filename.trim()
      : undefined;

    // SECURITY: Validate the path to prevent path traversal attacks
    const basePathValidation = validateSnapshotPath(rawPath);
    if (!basePathValidation.isValid) {
      return {
        success: false,
        error: basePathValidation.error
      } as StandardActionResponse;
    }

    let targetPath: string;
    if (rawFilename) {
      // Validate filename doesn't contain path traversal
      if (rawFilename.includes('..') || rawFilename.includes('/') || rawFilename.includes('\\')) {
        return {
          success: false,
          error: 'SECURITY_VIOLATION: Filename cannot contain path separators or traversal'
        } as StandardActionResponse;
      }
      targetPath = path.join(basePathValidation.safePath, rawFilename);
    } else {
      const hasExt = /\.[a-z0-9]+$/i.test(rawPath);
      if (hasExt) {
        targetPath = basePathValidation.safePath;
      } else {
        const filename = 'env_snapshot.json';
        targetPath = path.join(basePathValidation.safePath, filename);
      }
    }

    // SECURITY: Final validation of the complete target path
    const finalValidation = validateSnapshotPath(targetPath);
    if (!finalValidation.isValid) {
      return {
        success: false,
        error: finalValidation.error
      } as StandardActionResponse;
    }

    try {
      let parsed: Record<string, unknown> | undefined = undefined;
      try {
        const contents = await fs.readFile(targetPath, 'utf8');
        try {
          parsed = JSON.parse(contents);
        } catch {
          parsed = undefined;
        }
      } catch (err: unknown) {
        const errObj = err as Record<string, unknown> | null;
        if (errObj && (errObj.code === 'ENOENT' || errObj.code === 'ENOTDIR')) {
          return {
            success: true,
            message: `Environment snapshot file not found at ${targetPath}; import treated as no-op`
          } as StandardActionResponse;
        }
        throw err;
      }

      return {
        success: true,
        message: `Environment snapshot import handled from ${targetPath}`,
        details: parsed && typeof parsed === 'object' ? parsed as Record<string, unknown> : undefined
      } as StandardActionResponse;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to import environment snapshot: ${message}`
      } as StandardActionResponse;
    }
  }

  async cleanup(params?: { names?: unknown; name?: unknown }): Promise<StandardActionResponse> {
    try {
      const rawNames = Array.isArray(params?.names) ? params.names : [];
      if (typeof params?.name === 'string' && params.name.trim().length > 0) {
        rawNames.push(params.name);
      }
      const cleaned = rawNames
        .filter((name): name is string => typeof name === 'string')
        .map((name) => name.trim())
        .filter((name) => name.length > 0);

      if (cleaned.length === 0) {
        return {
          success: true,
          message: 'No environment actor names provided for cleanup; no-op'
        } as StandardActionResponse;
      }

      const bridge = this.ensureAutomationBridge();
      const resp = await bridge.sendAutomationRequest('build_environment', {
        action: 'delete',
        names: cleaned
      }, { timeoutMs: 40000 }) as Record<string, unknown>;

      if (!resp || resp.success === false) {
        const result = resp && typeof resp.result === 'object' ? resp.result as Record<string, unknown> : undefined;
        return {
          success: false,
          error: typeof resp?.error === 'string' && resp.error.length > 0
            ? resp.error
            : (typeof resp?.message === 'string' && resp.message.length > 0
              ? resp.message
              : 'Failed to delete environment actors'),
          message: typeof resp?.message === 'string' ? resp.message : undefined,
          details: result
        } as StandardActionResponse;
      }

      const result = resp && typeof resp.result === 'object' ? resp.result as Record<string, unknown> : undefined;

      return {
        success: true,
        message: typeof resp.message === 'string' && resp.message.length > 0
          ? resp.message
          : `Environment actors deleted: ${cleaned.join(', ')}`,
        details: result ?? { names: cleaned }
      } as StandardActionResponse;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to cleanup environment actors: ${message}`
      } as StandardActionResponse;
    }
  }
}
