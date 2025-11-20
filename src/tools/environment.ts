import { AutomationBridge } from '../automation-bridge.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { DEFAULT_SKYLIGHT_INTENSITY, DEFAULT_SUN_INTENSITY, DEFAULT_TIME_OF_DAY } from '../constants.js';

interface EnvironmentResult {
  success: boolean;
  message?: string;
  error?: string;
  details?: Record<string, unknown>;
}

export class EnvironmentTools {
  constructor(_bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

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
      const response = await bridge.controlEnvironment(action, payload, { timeoutMs: 40000 });
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
}
