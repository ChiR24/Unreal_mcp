import { UnrealBridge } from '../unreal-bridge.js';
import { loadEnv } from '../types/env.js';
import { spawn } from 'child_process';

export class EngineTools {
  private env = loadEnv();
  constructor(private bridge: UnrealBridge) {}

  async launchEditor(params?: { editorExe?: string; projectPath?: string }) {
    const exe = params?.editorExe || this.env.UE_EDITOR_EXE;
    const proj = params?.projectPath || this.env.UE_PROJECT_PATH;
    if (!exe) return { success: false, error: 'UE_EDITOR_EXE not set and editorExe not provided' };
    if (!proj) return { success: false, error: 'UE_PROJECT_PATH not set and projectPath not provided' };
    try {
      const child = spawn(exe, [proj], { detached: true, stdio: 'ignore' });
      child.unref();
      return { success: true, pid: child.pid, message: 'Editor launch requested' };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }

  async quitEditor() {
    try {
      // Use Python SystemLibrary.quit_editor if available
      await this.bridge.executePython('import unreal; unreal.SystemLibrary.quit_editor()');
      return { success: true, message: 'Quit command sent' };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }
}
