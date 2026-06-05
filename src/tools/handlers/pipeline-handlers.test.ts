import { describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import type { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { handlePipelineTools } from './pipeline-handlers.js';

const tools = {} as unknown as ITools;

function runUbt(args: Partial<PipelineArgs>) {
  return handlePipelineTools('run_ubt', {
    target: 'MCPtestEditor',
    platform: 'Linux',
    configuration: 'Development',
    ...args
  } as PipelineArgs, tools);
}

function platformFolder(): string {
  if (process.platform === 'win32') return process.arch === 'arm64' ? 'win-arm64' : 'win-x64';
  if (process.platform === 'darwin') return process.arch === 'arm64' ? 'mac-arm64' : 'mac-x64';
  return process.arch === 'arm64' ? 'linux-arm64' : 'linux-x64';
}

function restoreEnv(key: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[key];
  } else {
    process.env[key] = value;
  }
}

describe('handlePipelineTools run_ubt validation', () => {
  it('rejects switch-shaped positional fields before local or bridge execution', async () => {
    await expect(runUbt({ target: '-Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/positional UBT token/);
  });

  it('rejects platform and configuration values outside allowlists', async () => {
    await expect(runUbt({ platform: 'Windows' }))
      .rejects.toThrow(/platform is not allowed/);

    await expect(runUbt({ configuration: 'DevelopmentEditor' }))
      .rejects.toThrow(/configuration is not allowed/);
  });

  it('rejects extra arguments that override the managed invocation', async () => {
    await expect(runUbt({ arguments: '-Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/cannot override/);

    await expect(runUbt({ arguments: '--Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/cannot override/);

    await expect(runUbt({ arguments: '@/tmp/ubt.rsp' }))
      .rejects.toThrow(/response-file/);
  });

  it('uses Unreal bundled dotnet when UBT is discovered as the legacy dll', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'cosmic-wolf-ubt-'));
    const previousEnginePath = process.env.UE_ENGINE_PATH;
    const previousProjectPath = process.env.UE_PROJECT_PATH;
    const previousCapturePath = process.env.DOTNET_CAPTURE_PATH;

    try {
      const enginePath = path.join(tempRoot, 'Engine');
      const ubtPath = path.join(enginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.dll');
      const dotnetRoot = path.join(enginePath, 'Binaries', 'ThirdParty', 'DotNet', '8.0.0', platformFolder());
      const dotnetPath = path.join(dotnetRoot, process.platform === 'win32' ? 'dotnet.exe' : 'dotnet');
      const projectPath = path.join(tempRoot, 'Game', 'MCPtest.uproject');
      const capturePath = path.join(tempRoot, 'dotnet-capture.json');

      await fs.mkdir(path.dirname(ubtPath), { recursive: true });
      await fs.mkdir(dotnetRoot, { recursive: true });
      await fs.mkdir(path.dirname(projectPath), { recursive: true });
      await fs.writeFile(ubtPath, 'fake ubt dll');
      await fs.writeFile(projectPath, '{"FileVersion":3}');
      await fs.writeFile(dotnetPath, [
        '#!/usr/bin/env node',
        "const fs = require('node:fs');",
        'fs.writeFileSync(process.env.DOTNET_CAPTURE_PATH, JSON.stringify({',
        '  argv: process.argv.slice(2),',
        '  dotnetRoot: process.env.DOTNET_ROOT,',
        '  path: process.env.PATH',
        '}));'
      ].join('\n'));
      await fs.chmod(dotnetPath, 0o755);

      process.env.UE_ENGINE_PATH = enginePath;
      delete process.env.UE_PROJECT_PATH;
      process.env.DOTNET_CAPTURE_PATH = capturePath;

      const result = await runUbt({ projectPath });
      const capture = JSON.parse(await fs.readFile(capturePath, 'utf-8')) as Record<string, unknown>;

      expect(result).toMatchObject({ success: true });
      expect(capture.dotnetRoot).toBe(dotnetRoot);
      expect(capture.argv).toEqual(expect.arrayContaining([ubtPath, 'MCPtestEditor', 'Linux', 'Development']));
    } finally {
      restoreEnv('UE_ENGINE_PATH', previousEnginePath);
      restoreEnv('UE_PROJECT_PATH', previousProjectPath);
      restoreEnv('DOTNET_CAPTURE_PATH', previousCapturePath);
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});
