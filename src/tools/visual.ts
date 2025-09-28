import { UnrealBridge } from '../unreal-bridge.js';
import { loadEnv } from '../types/env.js';
import { Logger } from '../utils/logger.js';
import { coerceStringArray, interpretStandardResult } from '../utils/result-helpers.js';
import { promises as fs } from 'fs';
import path from 'path';

export class VisualTools {
  private env = loadEnv();
  private log = new Logger('VisualTools');
  constructor(private bridge: UnrealBridge) {}

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
        return { success: true, message: 'Screenshot triggered, but could not locate output file' };
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
    const python = `
import unreal
import json
import os

result = {
  "success": True,
  "message": "",
  "error": "",
  "directories": [],
  "warnings": [],
  "details": []
}

def finalize():
  data = dict(result)
  if data.get("success"):
    if not data.get("message"):
      data["message"] = "Collected screenshot directories"
    data.pop("error", None)
  else:
    if not data.get("error"):
      data["error"] = data.get("message") or "Failed to collect screenshot directories"
    if not data.get("message"):
      data["message"] = data["error"]
  if not data.get("warnings"):
    data.pop("warnings", None)
  if not data.get("details"):
    data.pop("details", None)
  if not data.get("directories"):
    data.pop("directories", None)
  return data

def add_path(candidate, note=None):
  if not candidate:
    return
  try:
    abs_path = os.path.abspath(os.path.normpath(candidate))
  except Exception as normalize_error:
    result["warnings"].append(f"Failed to normalize path {candidate}: {normalize_error}")
    return
  if abs_path not in result["directories"]:
    result["directories"].append(abs_path)
    if note:
      result["details"].append(f"{note}: {abs_path}")
    else:
      result["details"].append(f"Discovered screenshot directory: {abs_path}")

try:
  automation_dir = unreal.AutomationLibrary.get_screenshot_directory()
  add_path(automation_dir, "Automation screenshot directory")
except Exception as automation_error:
  result["warnings"].append(f"Automation screenshot directory unavailable: {automation_error}")

try:
  project_saved = unreal.Paths.project_saved_dir()
  add_path(project_saved, "Project Saved directory")
  if project_saved:
    add_path(os.path.join(project_saved, 'Screenshots'), "Project Saved screenshots")
    add_path(os.path.join(project_saved, 'Screenshots', 'Windows'), "Project Saved Windows screenshots")
    add_path(os.path.join(project_saved, 'Screenshots', 'WindowsEditor'), "Project Saved WindowsEditor screenshots")
    try:
      platform_name = unreal.SystemLibrary.get_platform_user_name()
      if platform_name:
        add_path(os.path.join(project_saved, 'Screenshots', platform_name), f"Project Saved screenshots for {platform_name}")
        add_path(os.path.join(project_saved, 'Screenshots', f"{platform_name}Editor"), f"Project Saved editor screenshots for {platform_name}")
    except Exception as platform_error:
      result["warnings"].append(f"Failed to resolve platform-specific screenshot directories: {platform_error}")
except Exception as saved_error:
  result["warnings"].append(f"Project Saved directory unavailable: {saved_error}")

try:
  project_file = unreal.Paths.get_project_file_path()
  if project_file:
    project_dir = os.path.dirname(project_file)
    add_path(os.path.join(project_dir, 'Saved', 'Screenshots'), "Project directory screenshots")
    add_path(os.path.join(project_dir, 'Saved', 'Screenshots', 'Windows'), "Project directory Windows screenshots")
    add_path(os.path.join(project_dir, 'Saved', 'Screenshots', 'WindowsEditor'), "Project directory WindowsEditor screenshots")
except Exception as project_error:
  result["warnings"].append(f"Project directory screenshots unavailable: {project_error}")

if not result["directories"]:
  result["warnings"].append("No screenshot directories discovered")

print('RESULT:' + json.dumps(finalize()))
`.trim()
  .replace(/\r?\n/g, '\n');

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Collected screenshot directories',
        failureMessage: 'Failed to collect screenshot directories'
      });

      if (interpreted.details) {
        for (const entry of interpreted.details) {
          this.log.debug(entry);
        }
      }

      if (interpreted.warnings) {
        for (const warning of interpreted.warnings) {
          this.log.debug(`Screenshot directory warning: ${warning}`);
        }
      }

      const directories = coerceStringArray(interpreted.payload.directories);
      if (directories?.length) {
        return directories;
      }

      if (!interpreted.success && interpreted.error) {
        this.log.warn(`Screenshot path probe failed: ${interpreted.error}`);
      }

      if (interpreted.rawText) {
        try {
          const fallback = JSON.parse(interpreted.rawText);
          const fallbackDirs = coerceStringArray(fallback);
          if (fallbackDirs?.length) {
            return fallbackDirs;
          }
        } catch {}
      }
    } catch (err) {
      this.log.debug('Screenshot path probe failed', err);
    }
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

    // Fallback: common locations relative to current working directory
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
}
