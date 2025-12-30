import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { coerceString } from '../utils/result-helpers.js';

export class LevelResources {
  private automationBridge: AutomationBridge | undefined;

  constructor(bridge: UnrealBridge) {
    this.automationBridge = (bridge as unknown as { automationBridge?: AutomationBridge }).automationBridge;
  }

  async getCurrentLevel() {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp = await this.automationBridge.sendAutomationRequest('get_level', { action: 'get_current' }) as Record<string, unknown>;
      const resultObj = resp?.result as Record<string, unknown> | null;
      if (resp && resp.success !== false && resultObj) {
        return {
          success: true,
          name: coerceString(resultObj.name) ?? 'None',
          path: coerceString(resultObj.path) ?? 'None'
        };
      }

      return { success: false, error: 'Failed to get current level' };
    } catch (err) {
      return { error: `Failed to get current level: ${err}`, success: false };
    }
  }

  async getLevelName() {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp = await this.automationBridge.sendAutomationRequest('get_level', { action: 'get_name' }) as Record<string, unknown>;
      const resultObj = resp?.result as Record<string, unknown> | null;
      if (resp && resp.success !== false && resultObj) {
        return {
          success: true,
          path: coerceString(resultObj.path) ?? ''
        };
      }

      return { success: false, error: 'Failed to get level name' };
    } catch (err) {
      return { error: `Failed to get level name: ${err}`, success: false };
    }
  }

  async saveCurrentLevel() {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp = await this.automationBridge.sendAutomationRequest('save_level', {}) as Record<string, unknown>;
      if (resp && resp.success !== false) {
        return { success: true, message: 'Level saved' };
      }

      return { success: false, error: 'Failed to save level' };
    } catch (err) {
      return { error: `Failed to save level: ${err}`, success: false };
    }
  }
}
