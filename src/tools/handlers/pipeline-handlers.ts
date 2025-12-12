import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';
import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';

export async function handlePipelineTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'run_ubt': {
      const target = args.target;
      const platform = args.platform || 'Win64';
      const configuration = args.configuration || 'Development';
      const extraArgs = args.arguments || '';

      if (!target) {
        return { success: false, error: 'MISSING_TARGET', message: 'Target is required for run_ubt' };
      }

      // Try to find UnrealBuildTool
      let ubtPath = 'UnrealBuildTool'; // Assume in PATH by default
      const enginePath = process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;

      if (enginePath) {
        const possiblePath = path.join(enginePath, 'Engine', 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe');
        if (fs.existsSync(possiblePath)) {
          ubtPath = possiblePath;
        }
      }

      let projectPath = process.env.UE_PROJECT_PATH;
      if (!projectPath && args.projectPath) {
        projectPath = args.projectPath;
      }

      if (!projectPath) {
        return { success: false, error: 'MISSING_PROJECT_PATH', message: 'UE_PROJECT_PATH environment variable is not set and no projectPath argument was provided.' };
      }

      // If projectPath points to a .uproject file, use it. If it's a directory, look for a .uproject file.
      let uprojectFile = projectPath;
      if (!uprojectFile.endsWith('.uproject')) {
        // Find first .uproject in the directory
        try {
          const files = fs.readdirSync(projectPath);
          const found = files.find(f => f.endsWith('.uproject'));
          if (found) {
            uprojectFile = path.join(projectPath, found);
          }
        } catch (_e) {
          return { success: false, error: 'INVALID_PROJECT_PATH', message: `Could not read project directory: ${projectPath}` };
        }
      }

      const cmdArgs = [
        target,
        platform,
        configuration,
        `-Project="${uprojectFile}"`,
        extraArgs
      ].filter(Boolean);

      return new Promise((resolve) => {
        const child = spawn(ubtPath, cmdArgs, { shell: true });

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

          if (code === 0) {
            resolve({
              success: true,
              message: 'UnrealBuildTool finished successfully',
              output: stdout + truncatedNote,
              command: `${ubtPath} ${cmdArgs.join(' ')}`
            });
          } else {
            resolve({
              success: false,
              error: 'UBT_FAILED',
              message: `UnrealBuildTool failed with code ${code}`,
              output: stdout + truncatedNote,
              errorOutput: stderr + truncatedNote,
              command: `${ubtPath} ${cmdArgs.join(' ')}`
            });
          }
        });

        child.on('error', (err) => {
          resolve({
            success: false,
            error: 'SPAWN_FAILED',
            message: `Failed to spawn UnrealBuildTool: ${err.message}`,
            command: `${ubtPath} ${cmdArgs.join(' ')}`
          });
        });
      });
    }
    default:
      // Fallback to automation bridge if we add more actions later that are bridge-supported
      const res = await executeAutomationRequest(tools, 'manage_pipeline', { ...args, subAction: action }, 'Automation bridge not available for manage_pipeline');
      return cleanObject(res);
  }
}
