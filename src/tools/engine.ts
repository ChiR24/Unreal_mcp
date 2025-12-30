import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { loadEnv } from '../types/env.js';
import { spawn } from 'child_process';

export class EngineTools {
  private env = loadEnv();
  constructor(_bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  async launchEditor(params?: { editorExe?: string; projectPath?: string }) {
    const exe = params?.editorExe || this.env.UE_EDITOR_EXE;
    const proj = params?.projectPath || this.env.UE_PROJECT_PATH;
    if (!exe) return { success: false, error: 'UE_EDITOR_EXE not set and editorExe not provided' };
    if (!proj) return { success: false, error: 'UE_PROJECT_PATH not set and projectPath not provided' };
    try {
      const child = spawn(exe, [proj], { detached: true, stdio: 'ignore' });
      child.unref();
      return { success: true, pid: child.pid, message: 'Editor launch requested' };
    } catch (err: unknown) {
      const errObj = err as Record<string, unknown> | null;
      return { success: false, error: String(errObj?.message || err) };
    }
  }

  async quitEditor() {
    if (!this.automationBridge) {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available for quit_editor' };
    }

    try {
      const resp = await this.automationBridge.sendAutomationRequest('quit_editor', {}) as Record<string, unknown> | null;
      if (resp && resp.success === false) {
        return { success: false, error: (resp.error as string) || (resp.message as string) || 'Quit request failed' };
      }
      return { success: true, message: 'Quit command sent' };
    } catch (err: unknown) {
      const errObj = err as Record<string, unknown> | null;
      return { success: false, error: String(errObj?.message || err) };
    }
  }
}
