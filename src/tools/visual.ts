import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { loadEnv } from '../types/env.js';
import { Logger } from '../utils/logger.js';
import { promises as fs } from 'fs';
import path from 'path';

export class VisualTools {
  private env = loadEnv();
  private log = new Logger('VisualTools');
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  // Take a screenshot of viewport (high res or standard). Returns path and base64 (truncated)
  async takeScreenshot(params: { resolution?: string }) {
    const res = params.resolution && /^\d+x\d+$/i.test(params.resolution) ? params.resolution : '';
  const primaryCommand = res ? `HighResShot ${res}` : 'Shot';
    const captureStartedAt = Date.now();
    try {
      const firstAttempt = await this.bridge.executeConsoleCommand(primaryCommand);
      const firstSummary = this.bridge.summarizeConsoleCommand(primaryCommand, firstAttempt);

      if (!res) {
        const output = (firstSummary.output || '').toLowerCase();
        const badInput = output.includes('bad input') || output.includes('unrecognized command');
        const hasErrorLine = firstSummary.logLines?.some(line => /error:/i.test(line));
        if (badInput || hasErrorLine) {
          this.log.debug(`Screenshot primary command reported an error (${firstSummary.output || 'no output'})`);
        }
      }
      // Give the engine a moment to write the file (UE can flush asynchronously)
      await new Promise(r => setTimeout(r, 1200));
      const p = await this.findLatestScreenshot(captureStartedAt);
      if (!p) {
        this.log.warn('Screenshot captured but output file was not found');
        return { success: false, handled: true, message: 'handled: screenshot triggered but output file not found' } as any;
      }
      let b64: string | undefined;
      let truncated = false;
      let sizeBytes: number | undefined;
      let mime: string | undefined;
      try {
        const stat = await fs.stat(p);
        sizeBytes = stat.size;
        const max = 1024 * 1024; // 1 MiB cap for inline payloads
        if (stat.size <= max) {
          const buf = await fs.readFile(p);
          b64 = buf.toString('base64');
        } else {
          truncated = true;
        }
        const ext = path.extname(p).toLowerCase();
        if (ext === '.png') mime = 'image/png';
        else if (ext === '.jpg' || ext === '.jpeg') mime = 'image/jpeg';
        else if (ext === '.bmp') mime = 'image/bmp';
      } catch {}
      return {
        success: true,
        imagePath: p.replace(/\\/g, '/'),
        imageMimeType: mime,
        imageSizeBytes: sizeBytes,
        imageBase64: b64,
        imageBase64Truncated: truncated
      };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }

  private async getEngineScreenshotDirectories(): Promise<string[]> {
    // Return empty array - screenshot directory discovery will use file system paths
    // from UE_PROJECT_PATH env variable instead
    return [];
  }

  private async findLatestScreenshot(since?: number): Promise<string | null> {
    // Try env override, otherwise look in common UE Saved/Screenshots folder under project
    const candidates: string[] = [];
    const seen = new Set<string>();
    const addCandidate = (candidate?: string | null) => {
      if (!candidate) return;
      const normalized = path.isAbsolute(candidate)
        ? path.normalize(candidate)
        : path.resolve(candidate);
      if (!seen.has(normalized)) {
        seen.add(normalized);
        candidates.push(normalized);
      }
    };

    if (this.env.UE_SCREENSHOT_DIR) addCandidate(this.env.UE_SCREENSHOT_DIR);
    if (this.env.UE_PROJECT_PATH) {
      const projectDir = path.dirname(this.env.UE_PROJECT_PATH);
      addCandidate(path.join(projectDir, 'Saved', 'Screenshots'));
      addCandidate(path.join(projectDir, 'Saved', 'Screenshots', 'Windows'));
      addCandidate(path.join(projectDir, 'Saved', 'Screenshots', 'WindowsEditor'));
    }

    const engineDirs = await this.getEngineScreenshotDirectories();
    for (const dir of engineDirs) {
      addCandidate(dir);
    }

  // Alternate: common locations relative to current working directory
    addCandidate(path.join(process.cwd(), 'Saved', 'Screenshots'));
    addCandidate(path.join(process.cwd(), 'Saved', 'Screenshots', 'Windows'));
    addCandidate(path.join(process.cwd(), 'Saved', 'Screenshots', 'WindowsEditor'));

    const searchDirs = new Set<string>();
    const queue: string[] = [...candidates];
    while (queue.length) {
      const candidate = queue.pop();
      if (!candidate || searchDirs.has(candidate)) continue;
      searchDirs.add(candidate);
      try {
        const entries = await fs.readdir(candidate, { withFileTypes: true });
        for (const entry of entries) {
          if (entry.isDirectory()) {
            queue.push(path.join(candidate, entry.name));
          }
        }
      } catch {}
    }

    let latest: { path: string; mtime: number } | null = null;
    let latestSince: { path: string; mtime: number } | null = null;
    const cutoff = since ? since - 2000 : undefined; // allow slight clock drift

    for (const dirPath of searchDirs) {
      let entries: string[];
      try {
        entries = await fs.readdir(dirPath);
      } catch {
        continue;
      }
      for (const entry of entries) {
        const fp = path.join(dirPath, entry);
        if (!/\.(png|jpg|jpeg|bmp)$/i.test(fp)) continue;
        try {
          const st = await fs.stat(fp);
          const info = { path: fp, mtime: st.mtimeMs };
          if (!latest || info.mtime > latest.mtime) {
            latest = info;
          }
          if (cutoff !== undefined && st.mtimeMs >= cutoff) {
            if (!latestSince || info.mtime > latestSince.mtime) {
              latestSince = info;
            }
          }
        } catch {}
      }
    }
    const chosen = latestSince || latest;
    return chosen?.path || null;
  }

  async cleanupActors(filter: string) {
    if (!filter || typeof filter !== 'string' || filter.trim().length === 0) {
      return { success: false, error: 'filter required' };
    }
    const cleaned = filter.trim();

    if (!this.automationBridge) {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'handled: cleanup not available without automation bridge' } as any;
    }

    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('cleanup', { filter: cleaned });
      if (resp && resp.success !== false) {
        return { 
          success: true, 
          removed: resp.removed ?? resp.result?.removed ?? 0, 
          removedActors: resp.removedActors ?? resp.result?.removedActors,
          message: resp.message || `Cleanup removed ${resp.removed ?? resp.result?.removed ?? 0} actors`
        };
      }
      return { 
        success: false, 
        error: resp?.error ?? 'CLEANUP_FAILED',
        message: resp?.message ?? 'Cleanup operation failed'
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }
}
