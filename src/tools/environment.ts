import fs from 'node:fs/promises';
import path from 'node:path';
import { AutomationBridge } from '../automation/index.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { DEFAULT_SKYLIGHT_INTENSITY, DEFAULT_SUN_INTENSITY, DEFAULT_TIME_OF_DAY } from '../constants.js';

interface EnvironmentResult {
  success: boolean;
  message?: string;
  error?: string;
  details?: Record<string, unknown>;
}

export class EnvironmentTools {
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

  private async invoke(action: string, payload: Record<string, unknown>): Promise<EnvironmentResult> {
    try {
      const bridge = this.ensureAutomationBridge();
      const response = await bridge.sendAutomationRequest(action, payload, { timeoutMs: 40000 });
      if (response.success === false) {
        return {
          success: false,
          error: typeof response.error === 'string' && response.error.length > 0 ? response.error : 'ENVIRONMENT_CONTROL_FAILED',
          message: typeof response.message === 'string' ? response.message : undefined,
          details: typeof response.result === 'object' && response.result ? response.result as Record<string, unknown> : undefined
        };
      }
      const resultPayload = typeof response.result === 'object' && response.result ? response.result as Record<string, unknown> : undefined;
      return {
        success: true,
        message: typeof response.message === 'string' ? response.message : 'Environment control action succeeded',
        details: resultPayload
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: message || 'ENVIRONMENT_CONTROL_FAILED'
      };
    }
  }

  async setTimeOfDay(hour: unknown): Promise<EnvironmentResult> {
    const normalizedHour = this.normalizeNumber(hour, 'hour', this.getDefaultTimeOfDay());
    const clampedHour = Math.min(Math.max(normalizedHour, 0), 24);
    return this.invoke('set_time_of_day', { hour: clampedHour });
  }

  async setSunIntensity(intensity: unknown): Promise<EnvironmentResult> {
    const normalized = this.normalizeNumber(intensity, 'intensity', this.getDefaultSunIntensity());
    const finalValue = Math.max(normalized, 0);
    return this.invoke('set_sun_intensity', { intensity: finalValue });
  }

  async setSkylightIntensity(intensity: unknown): Promise<EnvironmentResult> {
    const normalized = this.normalizeNumber(intensity, 'intensity', this.getDefaultSkylightIntensity());
    const finalValue = Math.max(normalized, 0);
    return this.invoke('set_skylight_intensity', { intensity: finalValue });
  }

  async exportSnapshot(params: { path?: unknown; filename?: unknown }): Promise<EnvironmentResult> {
    try {
      const rawPath = typeof params?.path === 'string' && params.path.trim().length > 0
        ? params.path.trim()
        : './tests/reports/env_snapshot.json';
      const rawFilename = typeof params?.filename === 'string' && params.filename.trim().length > 0
        ? params.filename.trim()
        : undefined;

      let targetPath: string;
      if (rawFilename) {
        const dir = rawPath;
        targetPath = path.isAbsolute(dir)
          ? path.join(dir, rawFilename)
          : path.join(process.cwd(), dir, rawFilename);
      } else {
        const hasExt = /\.[a-z0-9]+$/i.test(rawPath);
        if (hasExt) {
          targetPath = path.isAbsolute(rawPath)
            ? rawPath
            : path.join(process.cwd(), rawPath);
        } else {
          const dir = rawPath;
          const filename = 'env_snapshot.json';
          targetPath = path.isAbsolute(dir)
            ? path.join(dir, filename)
            : path.join(process.cwd(), dir, filename);
        }
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
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to export environment snapshot: ${message}`
      };
    }
  }

  async importSnapshot(params: { path?: unknown; filename?: unknown }): Promise<EnvironmentResult> {
    const rawPath = typeof params?.path === 'string' && params.path.trim().length > 0
      ? params.path.trim()
      : './tests/reports/env_snapshot.json';
    const rawFilename = typeof params?.filename === 'string' && params.filename.trim().length > 0
      ? params.filename.trim()
      : undefined;

    let targetPath: string;
    if (rawFilename) {
      const dir = rawPath;
      targetPath = path.isAbsolute(dir)
        ? path.join(dir, rawFilename)
        : path.join(process.cwd(), dir, rawFilename);
    } else {
      const hasExt = /\.[a-z0-9]+$/i.test(rawPath);
      if (hasExt) {
        targetPath = path.isAbsolute(rawPath)
          ? rawPath
          : path.join(process.cwd(), rawPath);
      } else {
        const dir = rawPath;
        const filename = 'env_snapshot.json';
        targetPath = path.isAbsolute(dir)
          ? path.join(dir, filename)
          : path.join(process.cwd(), dir, filename);
      }
    }

    try {
      let parsed: any = undefined;
      try {
        const contents = await fs.readFile(targetPath, 'utf8');
        try {
          parsed = JSON.parse(contents);
        } catch {
          parsed = undefined;
        }
      } catch (err: any) {
        if (err && (err.code === 'ENOENT' || err.code === 'ENOTDIR')) {
          return {
            success: true,
            message: `Environment snapshot file not found at ${targetPath}; import treated as no-op`
          };
        }
        throw err;
      }

      return {
        success: true,
        message: `Environment snapshot import handled from ${targetPath}`,
        details: parsed && typeof parsed === 'object' ? parsed as Record<string, unknown> : undefined
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to import environment snapshot: ${message}`
      };
    }
  }

  async cleanup(params?: { names?: unknown; name?: unknown }): Promise<EnvironmentResult> {
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
        };
      }

      const bridge = this.ensureAutomationBridge();
      const resp: any = await bridge.sendAutomationRequest('build_environment', {
        action: 'delete',
        names: cleaned
      }, { timeoutMs: 40000 });

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
        };
      }

      const result = resp && typeof resp.result === 'object' ? resp.result as Record<string, unknown> : undefined;

      return {
        success: true,
        message: typeof resp.message === 'string' && resp.message.length > 0
          ? resp.message
          : `Environment actors deleted: ${cleaned.join(', ')}`,
        details: result ?? { names: cleaned }
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: `Failed to cleanup environment actors: ${message}`
      };
    }
  }
}
