import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { coerceString } from '../utils/result-helpers.js';

export class LevelResources {
  private automationBridge: AutomationBridge | undefined;

  constructor(_bridge: UnrealBridge, automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async getCurrentLevel() {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        return { success: false, error: 'Automation bridge is not available' };
      }

      // Use list_levels action which returns currentMap and currentMapPath
      const resp = await this.automationBridge.sendAutomationRequest('list_levels', {}) as Record<string, unknown>;
      if (resp && resp.success !== false) {
        // Check both top level and result for the data (response format varies)
        const result = resp.result as Record<string, unknown> | undefined;
        const name = coerceString(resp.currentMap) ?? coerceString(result?.currentMap) ?? 'None';
        const path = coerceString(resp.currentMapPath) ?? coerceString(result?.currentMapPath) ?? 'None';
        return { success: true, name, path };
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

      // Use list_levels action which returns currentMapPath
      const resp = await this.automationBridge.sendAutomationRequest('list_levels', {}) as Record<string, unknown>;
      if (resp && resp.success !== false) {
        // Check both top level and result for the data
        const result = resp.result as Record<string, unknown> | undefined;
        return {
          success: true,
          path: coerceString(resp.currentMapPath) ?? coerceString(result?.currentMapPath) ?? ''
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
