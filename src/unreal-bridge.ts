import type { AutomationBridge } from './automation/index.js';
import { CONSOLE_COMMAND_TIMEOUT_MS, ENGINE_QUERY_TIMEOUT_MS } from './constants.js';
import { CommandValidator } from './utils/commands/command-validator.js';
import { Logger } from './utils/logging/logger.js';
import { isRecord } from './utils/validation/type-guards.js';

export interface EngineVersionInfo {
  readonly version: string;
  readonly major: number;
  readonly minor: number;
  readonly patch: number;
  readonly isUE56OrAbove: boolean;
}

export interface FeatureFlagsInfo {
  readonly subsystems: {
    readonly unrealEditor: boolean;
    readonly levelEditor: boolean;
    readonly editorActor: boolean;
  };
}

const isMock = (): boolean => process.env.MOCK_UNREAL_CONNECTION === 'true';

// Bridge replies carry the engine's payload under `result`.
function payloadOf(response: unknown): Record<string, unknown> {
  if (isRecord(response) && isRecord(response.result)) return response.result;
  return isRecord(response) ? response : {};
}

/**
 * The server's view of the Unreal connection: whether it is up, an on-demand
 * connect (the automation bridge's own lazy connect), and the handful of engine
 * queries the resources and health ping make. Mock mode answers locally.
 */
export class UnrealBridge {
  private readonly log = new Logger('UnrealBridge');
  private automationBridge?: AutomationBridge;

  get isConnected(): boolean {
    return isMock() || this.automationBridge?.isConnected() === true;
  }

  setAutomationBridge(automationBridge?: AutomationBridge): void {
    this.automationBridge = automationBridge;
  }

  getAutomationBridge(): AutomationBridge {
    if (!this.automationBridge) throw new Error('Automation bridge is not configured');
    return this.automationBridge;
  }

  /** Connect now if needed; false when Unreal is unreachable. */
  async tryConnect(): Promise<boolean> {
    if (this.isConnected) return true;
    return (await this.automationBridge?.connect()) ?? false;
  }

  async executeConsoleCommand(command: string): Promise<unknown> {
    CommandValidator.validate(command);
    if (isMock()) return { success: true, message: `Mock execution of '${command.trim()}' successful` };
    const response = await this.getAutomationBridge().sendAutomationRequest(
      'console_command', { command: command.trim() }, { timeoutMs: CONSOLE_COMMAND_TIMEOUT_MS });
    if (isRecord(response) && response.success === false) {
      throw new Error(String(response.message ?? response.error ?? 'Console command failed'));
    }
    return response;
  }

  async getEngineVersion(): Promise<EngineVersionInfo> {
    if (isMock()) return { version: '5.6.0-Mock', major: 5, minor: 6, patch: 0, isUE56OrAbove: true };
    try {
      // engineVersion is FEngineVersion::ToString(), e.g. "5.7.1-46000000+++UE5+Release-5.7".
      const raw = payloadOf(await this.getAutomationBridge().sendAutomationRequest(
        'inspect', { action: 'get_project_settings' }, { timeoutMs: ENGINE_QUERY_TIMEOUT_MS }));
      const version = typeof raw.engineVersion === 'string' ? raw.engineVersion : 'unknown';
      const [major = 0, minor = 0, patch = 0] = (/^(\d+)\.(\d+)\.(\d+)/.exec(version)?.slice(1) ?? []).map(Number);
      return { version, major, minor, patch, isUE56OrAbove: major > 5 || (major === 5 && minor >= 6) };
    } catch (error) {
      this.log.warn('getEngineVersion failed', error instanceof Error ? error.message : String(error));
      return { version: 'unknown', major: 0, minor: 0, patch: 0, isUE56OrAbove: false };
    }
  }

  // The editor-only plugin runs in UnrealEditor, where these three subsystems always exist.
  getFeatureFlags(): FeatureFlagsInfo {
    const up = this.isConnected;
    return { subsystems: { unrealEditor: up, levelEditor: up, editorActor: up } };
  }
}
