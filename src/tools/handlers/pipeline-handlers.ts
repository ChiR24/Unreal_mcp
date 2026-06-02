import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { spawn, exec } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import util from 'node:util';

/** Promisified child_process.exec for async shell commands. */
const execAsync = util.promisify(exec);
const ALLOWED_UBT_PLATFORMS = new Set(['Win64', 'Mac', 'Linux', 'LinuxArm64', 'Android', 'IOS', 'TVOS', 'HoloLens', 'VisionOS']);
const ALLOWED_UBT_CONFIGURATIONS = new Set(['Debug', 'DebugGame', 'Development', 'Shipping', 'Test']);
const BLOCKED_UBT_OVERRIDE_OPTIONS = new Set(['project', 'projectfile', 'target', 'mode']);

/** Reject UBT argument strings containing shell-dangerous characters. */
function validateUbtArgumentsString(extraArgs: string): void {
  if (!extraArgs || typeof extraArgs !== 'string') {
    return;
  }

  const forbiddenChars = ['\n', '\r', ';', '|', '`', '&&', '||', '>', '<', '"', "'"];
  for (const char of forbiddenChars) {
    if (extraArgs.includes(char)) {
      throw new Error(
        `UBT arguments contain forbidden character(s) and are blocked for safety. Blocked: ${JSON.stringify(char)}.`
      );
    }
  }

  for (const token of tokenizeArgs(extraArgs)) {
    validateUbtExtraArgumentToken(token);
  }
}

function validateUbtArgumentToken(token: string, context: string): void {
  if (!token || token.trim().length === 0) {
    throw new Error(`${context} must be a non-empty UBT token.`);
  }

  const trimmed = token.trim();
  if (!/^[A-Za-z0-9_\-.=:/\\+]+$/.test(trimmed)) {
    throw new Error(`${context} contains unsafe UBT argument characters.`);
  }
}

function validateUbtPositionalToken(token: string, context: string): void {
  validateUbtArgumentToken(token, context);
  const trimmed = token.trim();
  if (/^[-/@]/.test(trimmed) || trimmed.includes('=') || trimmed.includes(':') || trimmed.includes('/') || trimmed.includes('\\')) {
    throw new Error(`${context} must be a positional UBT token and cannot be a switch or path.`);
  }
}

function validateUbtTarget(target: string): void {
  validateUbtPositionalToken(target, 'run_ubt.target');
}

function validateUbtPlatform(platform: string): void {
  validateUbtPositionalToken(platform, 'run_ubt.platform');
  if (!ALLOWED_UBT_PLATFORMS.has(platform)) {
    throw new Error(`run_ubt.platform is not allowed: ${platform}`);
  }
}

function validateUbtConfiguration(configuration: string): void {
  validateUbtPositionalToken(configuration, 'run_ubt.configuration');
  if (!ALLOWED_UBT_CONFIGURATIONS.has(configuration)) {
    throw new Error(`run_ubt.configuration is not allowed: ${configuration}`);
  }
}

function getUbtOptionName(token: string): string | undefined {
  const trimmed = token.trim().toLowerCase();
  if (!trimmed.startsWith('-') && !trimmed.startsWith('/')) {
    return undefined;
  }

  const withoutPrefix = trimmed.replace(/^[-/]+/, '');
  const separatorIndex = withoutPrefix.search(/[=:]/);
  return separatorIndex >= 0 ? withoutPrefix.slice(0, separatorIndex) : withoutPrefix;
}

function validateUbtExtraArgumentToken(token: string): void {
  const trimmed = token.trim();
  if (trimmed.startsWith('@')) {
    throw new Error('UBT response-file arguments are blocked for safety.');
  }
  validateUbtArgumentToken(token, 'run_ubt.arguments');

  const optionName = getUbtOptionName(trimmed);
  if (optionName && BLOCKED_UBT_OVERRIDE_OPTIONS.has(optionName)) {
    throw new Error(`UBT argument ${optionName} cannot override the managed invocation.`);
  }
}

/** Split a UBT argument string into tokens, respecting quoted segments. */
function tokenizeArgs(extraArgs: string): string[] {
  if (!extraArgs) {
    return [];
  }

  const args: string[] = [];
  let current = '';
  let inQuotes = false;
  let escapeNext = false;

  for (let i = 0; i < extraArgs.length; i++) {
    const ch = extraArgs[i];

    if (escapeNext) {
      current += ch;
      escapeNext = false;
      continue;
    }

    if (ch === '\\') {
      escapeNext = true;
      continue;
    }

    if (ch === '"') {
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && /\s/.test(ch)) {
      if (current.length > 0) {
        args.push(current);
        current = '';
      }
    } else {
      current += ch;
    }
  }

  if (current.length > 0) {
    args.push(current);
  }

  return args;
}

/** Return true only when the final path segment is literally "Engine". */
function isEngineDirectoryPath(enginePath: string): boolean {
  const trimmed = enginePath.replace(/[\\/]+$/, '');
  const segments = trimmed.split(/[\\/]/);
  const lastSegment = segments[segments.length - 1];
  return typeof lastSegment === 'string' && lastSegment.toLowerCase() === 'engine';
}

/**
 * Probe a concrete UBT file path for existence + executability.
 * Returns the path if valid, undefined otherwise.
 */
async function tryUbtpath(candidate: string): Promise<string | undefined> {
  let mode = fs.constants.F_OK;
  if (process.platform !== 'win32') {
    // For non-Windows, require X_OK unless it's a .dll which dotnet executes
    if (!candidate.endsWith('.dll')) {
      mode = fs.constants.F_OK | fs.constants.X_OK;
    }
  }
  try {
    await fs.promises.access(candidate, mode);
    return candidate;
  } catch { /* not usable */ }
  return undefined;
}

/**
 * Resolve the UnrealBuildTool executable path using multiple discovery strategies.
 * Returns an empty string when not found — caller should delegate to C++ handler.
 */
async function findUbtExecutable(): Promise<string> {
  // ─── Strategy 1: Explicit environment variable ────────────────────────
  // UE_ENGINE_PATH is the convention in this project (see AGENTS.md / README).
  // The path may point to either:
  //   • .../UE_5.x/Engine    (already includes "Engine" suffix)
  //   • .../UE_5.x           (root directory without "Engine")
  const enginePath =
    process.env.UE_ENGINE_PATH ??
    process.env.UNREAL_ENGINE_PATH ??
    undefined;

  if (enginePath) {
    const endsWithEngine = isEngineDirectoryPath(enginePath);

    const roots: string[] = endsWithEngine
      ? [enginePath]
      : [path.join(enginePath, 'Engine')];

    for (const root of roots) {
      // Check all known UBT locations across UE 5.0 – 5.7:
      //   1. UE 5.4+ wrapper .exe  (Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe)
      //   2. UE 5.4+ directory .exe (Binaries/DotNET/UnrealBuildTool) — some versions
      //   3. Pre-5.4 .dll          (Binaries/DotNET/UnrealBuildTool.dll)
      const candidates = [
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
      ];
      for (const c of candidates) {
        const hit = await tryUbtpath(c);
        if (hit) return hit;
      }
    }
  }

  // ─── Strategy 2: Discover from .uproject EngineAssociation ────────────
  const projectPath = process.env.UE_PROJECT_PATH;
  if (projectPath) {
    let uprojectFile: string | undefined = undefined;
    if (projectPath.endsWith('.uproject')) {
      uprojectFile = projectPath;
    } else {
      try {
        const files = await fs.promises.readdir(projectPath);
        const found = files.find(f => f.endsWith('.uproject'));
        if (found) uprojectFile = path.join(projectPath, found);
      } catch { /* ignore */ }
    }

    if (uprojectFile) {
      try {
        const contentRaw = await fs.promises.readFile(uprojectFile, 'utf-8');
        const content = JSON.parse(contentRaw);
        const association = content.EngineAssociation as string | undefined;

        if (association) {
          const versionMatch = association.match(/^(\d+)\.(\d+)$/);
          if (versionMatch) {
            const [, major, minor] = versionMatch;
            const versionKey = `UE_${major}.${minor}`;

            const searchRoots: string[] = [];

            // Version-specific env vars
            const versionedEnvVars = [
              `${versionKey}_ROOT`,
              `${versionKey.replace('.', '_')}_ROOT`,
              `UE_ENGINE_PATH_${major}${minor}`,
              `UE${major}${minor}_ENGINE_PATH`,
            ];
            for (const key of versionedEnvVars) {
              const value = process.env[key];
              if (value) searchRoots.push(value);
            }

            // Standard Epic Launcher (Windows)
            searchRoots.push(
              path.join('C:', 'Program Files', 'Epic Games', versionKey, 'Engine'),
              path.join('E:', 'EpicGames', versionKey, 'Engine'),
            );

            // Known custom install layouts from this machine
            searchRoots.push(
              path.join('X:', 'Unreal_Engine', versionKey, 'Engine'),
              path.join('D:', 'Unreal_Engine', versionKey, 'Engine'),
            );

            for (const root of searchRoots) {
              if (!root) continue;
              const candidates = [
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
              ];
              for (const c of candidates) {
                const hit = await tryUbtpath(c);
                if (hit) return hit;
              }
            }
          }
        }

        // Fallback: check DefaultEngine.ini for EnginePath
        const iniPath = path.join(path.dirname(uprojectFile), 'Config', 'DefaultEngine.ini');
        try {
          const iniContent = await fs.promises.readFile(iniPath, 'utf-8');
          const iniMatch = iniContent.match(/EnginePath\s*=\s*(.+)/);
          if (iniMatch) {
            const iniEnginePath = iniMatch[1].trim().replace(/["']/g, '');
            const candidates = [
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
            ];
            for (const c of candidates) {
              const hit = await tryUbtpath(c);
              if (hit) return hit;
            }
          }
        } catch { /* no ini */ }
      } catch { /* uproject parse failed */ }
    }
  }

  // ─── Strategy 3: Global PATH lookup ───────────────────────────────────
  try {
    const whichCmd = process.platform === 'win32' ? 'where' : 'which';
    const { stdout } = await execAsync(`${whichCmd} UnrealBuildTool`, {
      encoding: 'utf-8',
      timeout: 5000,
    });
    if (stdout) {
      const first = stdout.trim().split(/\r?\n/)[0];
      if (first) return first;
    }
  } catch { /* not on PATH */ }

  // Not found — caller will delegate to the C++ bridge handler.
  return '';
}

/** Return Unreal's bundled .NET runtime folder for the current platform, if present. */
async function findBundledDotNetRoot(ubtPath: string): Promise<string | undefined> {
  const platformFolder = (() => {
    if (process.platform === 'win32') {
      return process.arch === 'arm64' ? 'win-arm64' : 'win-x64';
    }
    if (process.platform === 'darwin') {
      return process.arch === 'arm64' ? 'mac-arm64' : 'mac-x64';
    }
    return process.arch === 'arm64' ? 'linux-arm64' : 'linux-x64';
  })();

  let candidateRoot = path.dirname(ubtPath);
  for (let depth = 0; depth < 6; depth++) {
    const dotNetBase = path.join(candidateRoot, 'Binaries', 'ThirdParty', 'DotNet');

    try {
      const entries = await fs.promises.readdir(dotNetBase, { withFileTypes: true });
      const versionDirs = entries
        .filter(entry => entry.isDirectory())
        .map(entry => entry.name)
        .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }));

      for (const versionDir of versionDirs) {
        const runtimeRoot = path.join(dotNetBase, versionDir, platformFolder);
        const dotnetExecutable = path.join(runtimeRoot, process.platform === 'win32' ? 'dotnet.exe' : 'dotnet');
        const hit = await tryUbtpath(dotnetExecutable);
        if (hit) {
          return runtimeRoot;
        }
      }
    } catch { }

    const parent = path.dirname(candidateRoot);
    if (parent === candidateRoot) {
      break;
    }
    candidateRoot = parent;
  }

  return undefined;
}

/** Dispatch system_control pipeline actions to local UBT or the C++ bridge. */
export async function handlePipelineTools(action: string, args: PipelineArgs, tools: ITools) {
  switch (action) {
    case 'run_ubt': {
      const target = args.target;
      const platform = args.platform || 'Win64';
      const configuration = args.configuration || 'Development';
      const extraArgs = args.arguments || '';

      if (!target) {
        throw new Error('Target is required for run_ubt');
      }

      validateUbtTarget(target);
      validateUbtPlatform(platform);
      validateUbtConfiguration(configuration);
      validateUbtArgumentsString(extraArgs);

      const discoveredUbtPath = await findUbtExecutable();

      if (!discoveredUbtPath) {
        // UBT not found on TS side — delegate to C++ handler which uses
        // FPaths::EngineDir() and always knows the correct engine root.
        const res = await executeAutomationRequest(
          tools,
          'manage_pipeline',
          { ...args, subAction: action },
          'Automation bridge not available for run_ubt'
        );
        return cleanObject(res);
      }

      let projectPath = process.env.UE_PROJECT_PATH;
      if (!projectPath && args.projectPath) {
        projectPath = args.projectPath;
      }

      if (!projectPath) {
        throw new Error('UE_PROJECT_PATH environment variable is not set and no projectPath argument was provided.');
      }

      let uprojectFile = projectPath;
      if (!uprojectFile.endsWith('.uproject')) {
        try {
          const files = await fs.promises.readdir(projectPath);
          const found = files.find(f => f.endsWith('.uproject'));
          if (found) {
            uprojectFile = path.join(projectPath, found);
          }
        } catch (_e) {
          throw new Error(`Could not read project directory: ${projectPath}`);
        }
      }

      const projectArg = `-Project=${uprojectFile}`;
      const extraTokens = tokenizeArgs(extraArgs);

      const cmdArgs = [
        target,
        platform,
        configuration,
        projectArg,
        ...extraTokens
      ];

      // UE 5.4+ ships UBT as a .dll invoked via `dotnet`; earlier versions
      // provide a standalone .exe wrapper.
      const isDll = discoveredUbtPath.endsWith('.dll');
      const executable = isDll ? 'dotnet' : discoveredUbtPath;
      const actualArgs = isDll ? [discoveredUbtPath, ...cmdArgs] : cmdArgs;
      const bundledDotNetRoot = await findBundledDotNetRoot(discoveredUbtPath);
      const childEnv = bundledDotNetRoot
        ? {
          ...process.env,
          DOTNET_ROOT: bundledDotNetRoot,
          DOTNET_MULTILEVEL_LOOKUP: '0',
          PATH: `${bundledDotNetRoot}${path.delimiter}${process.env.PATH ?? ''}`,
        }
        : process.env;

      return new Promise((resolve) => {
        const child = spawn(executable, actualArgs, { shell: false, env: childEnv });

        const MAX_OUTPUT_SIZE = 20 * 1024; // 20KB cap
        let stdout = '';
        let stderr = '';

        child.stdout.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str);
          stdout += str;
          if (stdout.length > MAX_OUTPUT_SIZE) {
            stdout = stdout.substring(stdout.length - MAX_OUTPUT_SIZE);
          }
        });

        child.stderr.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str);
          stderr += str;
          if (stderr.length > MAX_OUTPUT_SIZE) {
            stderr = stderr.substring(stderr.length - MAX_OUTPUT_SIZE);
          }
        });

        child.on('close', (code) => {
          const truncatedNote = (stdout.length >= MAX_OUTPUT_SIZE || stderr.length >= MAX_OUTPUT_SIZE)
            ? '\n[Output truncated for response payload]'
            : '';

          const quotedArgs = cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg);

          if (code === 0) {
            resolve({
              success: true,
              message: 'UnrealBuildTool finished successfully',
              output: stdout + truncatedNote,
              command: `${executable} ${quotedArgs.join(' ')}`
            });
          } else {
            resolve({
              success: false,
              error: 'UBT_FAILED',
              message: `UnrealBuildTool failed with code ${code}`,
              output: stdout + truncatedNote,
              errorOutput: stderr + truncatedNote,
              command: `${executable} ${quotedArgs.join(' ')}`
            });
          }
        });

        child.on('error', (err) => {
          const quotedArgs = cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg);

          resolve({
            success: false,
            error: 'SPAWN_FAILED',
            message: `Failed to spawn UnrealBuildTool: ${err.message}`,
            command: `${executable} ${quotedArgs.join(' ')}`
          });
        });
      });
    }

    default: {
      return cleanObject({ success: false, error: 'UNKNOWN_ACTION', message: `Unknown system_control pipeline action: ${action}` });
    }
  }
}
