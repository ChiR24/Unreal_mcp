import { UnrealBridge } from '../unreal-bridge.js';
import { coerceString } from '../utils/result-helpers.js';

export class LevelResources {
  private automationBridge: any;

  constructor(bridge: UnrealBridge) {
    this.automationBridge = (bridge as any).automationBridge;
  }

  async getCurrentLevel() {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp: any = await this.automationBridge.sendAutomationRequest('get_level', { action: 'get_current' });
      if (resp && resp.success !== false && resp.result) {
        return {
          success: true,
          name: coerceString(resp.result.name) ?? 'None',
          path: coerceString(resp.result.path) ?? 'None'
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

      const resp: any = await this.automationBridge.sendAutomationRequest('get_level', { action: 'get_name' });
      if (resp && resp.success !== false && resp.result) {
        return {
          success: true,
          path: coerceString(resp.result.path) ?? ''
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

      const resp: any = await this.automationBridge.sendAutomationRequest('save_level', {});
      if (resp && resp.success !== false) {
        return { success: true, message: 'Level saved' };
      }

      return { success: false, error: 'Failed to save level' };
    } catch (err) {
      return { error: `Failed to save level: ${err}`, success: false };
    }
  }
}
