import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { coerceString } from '../utils/result-helpers.js';
import { isRecord } from '../utils/type-guards.js';

const LIST_LEVELS_ACTION = 'list_levels';
const BRIDGE_UNAVAILABLE = 'Automation bridge is not available';

export class LevelResources {
  private automationBridge: AutomationBridge | undefined;

  constructor(_bridge: UnrealBridge, automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async getCurrentLevel() {
    try {
      const resp = await this.sendAutomationRequest(LIST_LEVELS_ACTION);
      if (!resp) {
        return { success: false, error: BRIDGE_UNAVAILABLE };
      }

      if (resp.success !== false) {
        const result = this.getNestedResult(resp);
        const name = coerceString(resp.currentMap) ?? coerceString(result?.currentMap) ?? 'None';
        const path = coerceString(resp.currentMapPath) ?? coerceString(result?.currentMapPath) ?? 'None';
        return { success: true, name, path };
      }

      return { success: false, error: 'Failed to get current level' };
    } catch (err) {
      return { error: `Failed to get current level: ${this.getErrorMessage(err)}`, success: false };
    }
  }

  async getLevelName() {
    try {
      const resp = await this.sendAutomationRequest(LIST_LEVELS_ACTION);
      if (!resp) {
        return { success: false, error: BRIDGE_UNAVAILABLE };
      }

      if (resp.success !== false) {
        const result = this.getNestedResult(resp);
        return {
          success: true,
          path: coerceString(resp.currentMapPath) ?? coerceString(result?.currentMapPath) ?? ''
        };
      }

      return { success: false, error: 'Failed to get level name' };
    } catch (err) {
      return { error: `Failed to get level name: ${this.getErrorMessage(err)}`, success: false };
    }
  }

  async saveCurrentLevel() {
    try {
      const resp = await this.sendAutomationRequest('save_level');
      if (!resp) {
        return { success: false, error: BRIDGE_UNAVAILABLE };
      }

      if (resp.success !== false) {
        return { success: true, message: 'Level saved' };
      }

      return { success: false, error: 'Failed to save level' };
    } catch (err) {
      return { error: `Failed to save level: ${this.getErrorMessage(err)}`, success: false };
    }
  }

  private async sendAutomationRequest(action: string): Promise<Record<string, unknown> | undefined> {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return undefined;
    }

    const response = await this.automationBridge.sendAutomationRequest(action, {});
    return isRecord(response) ? response : {};
  }

  private getNestedResult(response: Record<string, unknown>): Record<string, unknown> | undefined {
    return isRecord(response.result) ? response.result : undefined;
  }

  private getErrorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
  }
}
