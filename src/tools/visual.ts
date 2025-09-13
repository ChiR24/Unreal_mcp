import { UnrealBridge } from '../unreal-bridge.js';
import { loadEnv } from '../types/env.js';
import fs from 'fs';
import path from 'path';

export class VisualTools {
  private env = loadEnv();
  constructor(private bridge: UnrealBridge) {}

  // Take a screenshot of viewport (high res or standard). Returns path and base64 (truncated)
  async takeScreenshot(params: { resolution?: string }) {
    const res = params.resolution && /^\d+x\d+$/i.test(params.resolution) ? params.resolution : '';
    const cmd = res ? `HighResShot ${res}` : 'HighResShot';
    try {
      await this.bridge.executeConsoleCommand(cmd);
      // Give the engine a moment to write the file
await new Promise(r => setTimeout(r, 1200));
      const p = await this.findLatestScreenshot();
      if (!p) return { success: true, message: 'Screenshot triggered, but could not locate output file' };
      let b64: string | undefined;
      try {
        const buf = fs.readFileSync(p);
        // Limit to ~1MB to avoid huge responses
        const max = 1024 * 1024;
        b64 = buf.length > max ? buf.subarray(0, max).toString('base64') : buf.toString('base64');
      } catch {}
      return { success: true, imagePath: p, imageBase64: b64 };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }

  private async findLatestScreenshot(): Promise<string | null> {
    // Try env override, otherwise look in common UE Saved/Screenshots folder under project
    const candidates: string[] = [];
    if (this.env.UE_SCREENSHOT_DIR) candidates.push(this.env.UE_SCREENSHOT_DIR);
    if (this.env.UE_PROJECT_PATH) {
      const projectDir = path.dirname(this.env.UE_PROJECT_PATH);
      candidates.push(path.join(projectDir, 'Saved', 'Screenshots'));
candidates.push(path.join(projectDir, 'Saved', 'Screenshots', 'Windows'));
      candidates.push(path.join(projectDir, 'Saved', 'Screenshots', 'WindowsEditor'));
    }
    // Fallback: common locations
candidates.push(path.join(process.cwd(), 'Saved', 'Screenshots'));
    candidates.push(path.join(process.cwd(), 'Saved', 'Screenshots', 'Windows'));
    candidates.push(path.join(process.cwd(), 'Saved', 'Screenshots', 'WindowsEditor'));
    let latest: { path: string; mtime: number } | null = null;
    for (const c of candidates) {
      try {
        const entries = fs.readdirSync(c).map(f => path.join(c, f));
        for (const fp of entries) {
          if (!/\.png$/i.test(fp)) continue;
          const st = fs.statSync(fp);
          if (!latest || st.mtimeMs > latest.mtime) latest = { path: fp, mtime: st.mtimeMs };
        }
      } catch {}
    }
    return latest?.path || null;
  }
}
