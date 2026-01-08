import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';

/**
 * Validates that a project path is safe and exists.
 * Prevents directory traversal attacks.
 */
function validateProjectPath(projectPath: string): { ok: true; normalized: string } | { ok: false; error: string } {
  if (!projectPath || typeof projectPath !== 'string') {
    return { ok: false, error: 'Project path must be a non-empty string' };
  }

  // Normalize path separators
  const normalized = path.normalize(projectPath);

  // Check for directory traversal attempts
  if (normalized.includes('..')) {
    return { ok: false, error: 'Directory traversal (..) is not allowed in project path' };
  }

  // Verify it's an absolute path
  if (!path.isAbsolute(normalized)) {
    return { ok: false, error: 'Project path must be an absolute path' };
  }

  // Verify the path exists
  if (!fs.existsSync(normalized)) {
    return { ok: false, error: `Project path does not exist: ${normalized}` };
  }

  // Verify it's a directory (unless it's a .uproject file)
  const stats = fs.statSync(normalized);
  if (!stats.isDirectory() && !normalized.endsWith('.uproject')) {
    return { ok: false, error: 'Project path must be a directory or .uproject file' };
  }

  return { ok: true, normalized };
}

function validateUbtArgumentsString(extraArgs: string): { ok: true } | { ok: false; error: string } {
  if (!extraArgs || typeof extraArgs !== 'string') {
    return { ok: true };
  }

  const forbiddenChars = ['\n', '\r', ';', '|', '`', '&&', '||', '>', '<'];
  for (const char of forbiddenChars) {
    if (extraArgs.includes(char)) {
      return {
        ok: false,
        error: `UBT arguments contain forbidden character(s) and are blocked for safety. Blocked: ${JSON.stringify(char)}.`
      };
    }
  }

  return { ok: true };
}

function tokenizeArgs(extraArgs: string): string[] {
  if (!extraArgs) {
    return [];
  }

  const args: string[] = [];
  let current = '';
  let inQuotes = false;
  let escapeNext = false;

  for (let i = 0; i < extraArgs.length; i++) {
    const ch = extraArgs[i] ?? '';

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

export async function handlePipelineTools(action: string, args: PipelineArgs, tools: ITools) {
  switch (action) {
    case 'run_ubt': {
      const target = args.target;
      const platform = args.platform || 'Win64';
      const configuration = args.configuration || 'Development';
      const extraArgs = args.arguments || '';

      if (!target) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'Target is required for run_ubt' });
      }
 
      const ubtArgsValidation = validateUbtArgumentsString(extraArgs);
      if (!ubtArgsValidation.ok) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: ubtArgsValidation.error });
      }

      let ubtPath = 'UnrealBuildTool';
      const enginePath = process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;

      if (enginePath) {
        const possiblePath = path.join(enginePath, 'Engine', 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe');
        if (fs.existsSync(possiblePath)) {
          ubtPath = possiblePath;
        }
      }

      let projectPath = process.env.UE_PROJECT_PATH;
      if (!projectPath && args.projectPath) {
        // Validate user-provided path to prevent directory traversal
        const pathValidation = validateProjectPath(args.projectPath);
        if (!pathValidation.ok) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: pathValidation.error
          });
        }
        projectPath = pathValidation.normalized;
      }

      if (!projectPath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'UE_PROJECT_PATH environment variable is not set and no projectPath argument was provided.'
        });
      }

      let uprojectFile = projectPath;
      if (!uprojectFile.endsWith('.uproject')) {
        try {
          const files = fs.readdirSync(projectPath);
          const found = files.find(f => f.endsWith('.uproject'));
          if (found) {
            uprojectFile = path.join(projectPath, found);
          }
        } catch (_e) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: `Could not read project directory: ${projectPath}`
          });
        }
      }

      const projectArg = `-Project="${uprojectFile}"`;
      const extraTokens = tokenizeArgs(extraArgs);

      const cmdArgs = [
        target,
        platform,
        configuration,
        projectArg,
        ...extraTokens
      ];

      return new Promise((resolve) => {
        const child = spawn(ubtPath, cmdArgs, { shell: false });

        const MAX_OUTPUT_SIZE = 20 * 1024; // 20KB cap
        let stdout = '';
        let stderr = '';

        child.stdout.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str); // Stream to server console (stderr to avoid MCP corruption)
          stdout += str;
          if (stdout.length > MAX_OUTPUT_SIZE) {
            stdout = stdout.substring(stdout.length - MAX_OUTPUT_SIZE);
          }
        });

        child.stderr.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str); // Stream to server console
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
              command: `${ubtPath} ${quotedArgs.join(' ')}`
            });
          } else {
            resolve({
              success: false,
              error: 'UBT_FAILED',
              message: `UnrealBuildTool failed with code ${code}`,
              output: stdout + truncatedNote,
              errorOutput: stderr + truncatedNote,
              command: `${ubtPath} ${quotedArgs.join(' ')}`
            });
          }
        });

        child.on('error', (err) => {
          const quotedArgs = cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg);

          resolve({
            success: false,
            error: 'SPAWN_FAILED',
            message: `Failed to spawn UnrealBuildTool: ${err.message}`,
            command: `${ubtPath} ${quotedArgs.join(' ')}`
          });
        });
      });
    }
    default:
      const res = await executeAutomationRequest(tools, 'manage_pipeline', { ...args, subAction: action }, 'Automation bridge not available for manage_pipeline');
      return cleanObject(res);
  }
}
