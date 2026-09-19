import { exec } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import util from 'node:util';
import { isRecord } from '../../../utils/validation/type-guards.js';
import { compareAscii } from '../../../utils/serialization/ordering.js';

// Newest .NET runtime directory first, without `localeCompare`: zero-padding each
// digit run makes a plain byte-order compare numeric-aware, so 10.0.1 outranks
// 9.0.1 (which raw byte order gets wrong) and the result never depends on ICU.
const dotNetVersionKey = (name: string): string =>
  name.replace(/\d+/g, (digits) => digits.padStart(10, '0'));

const execAsync = util.promisify(exec);

function isEngineDirectoryPath(enginePath: string): boolean {
  const trimmed = enginePath.replace(/[\\/]+$/, '');
  const segments = trimmed.split(/[\\/]/);
  const lastSegment = segments[segments.length - 1];
  return typeof lastSegment === 'string' && lastSegment.toLowerCase() === 'engine';
}

async function tryUbtPath(candidate: string): Promise<string | undefined> {
  let mode = fs.constants.F_OK;
  if (process.platform !== 'win32' && !candidate.endsWith('.dll')) {
    mode = fs.constants.F_OK | fs.constants.X_OK;
  }
  try {
    await fs.promises.access(candidate, mode);
    return candidate;
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
  }
  return undefined;
}

function getUbtCandidates(root: string): string[] {
  return [
    path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
    path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool'),
    path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
    path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
  ];
}

async function findFirstExistingCandidate(roots: string[]): Promise<string> {
  for (const root of roots) {
    for (const candidate of getUbtCandidates(root)) {
      const hit = await tryUbtPath(candidate);
      if (hit) return hit;
    }
  }
  return '';
}

async function findProjectFile(projectPath: string): Promise<string | undefined> {
  if (projectPath.endsWith('.uproject')) {
    return projectPath;
  }

  try {
    const files = await fs.promises.readdir(projectPath);
    const found = files.find(file => file.endsWith('.uproject'));
    return found ? path.join(projectPath, found) : undefined;
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
  }
  return undefined;
}

function getVersionedEngineRoots(major: string, minor: string): string[] {
  const versionKey = `UE_${major}.${minor}`;
  const searchRoots: string[] = [];
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

  searchRoots.push(
    path.join('C:', 'Program Files', 'Epic Games', versionKey, 'Engine'),
    path.join('E:', 'EpicGames', versionKey, 'Engine'),
    path.join('X:', 'Unreal_Engine', versionKey, 'Engine'),
    path.join('D:', 'Unreal_Engine', versionKey, 'Engine'),
  );

  return searchRoots;
}

async function findFromProjectAssociation(uprojectFile: string): Promise<string> {
  try {
    const contentRaw = await fs.promises.readFile(uprojectFile, 'utf-8');
    const content = JSON.parse(contentRaw);
    const association = isRecord(content) && typeof content.EngineAssociation === 'string'
      ? content.EngineAssociation
      : undefined;

    const versionMatch = association?.match(/^(\d+)\.(\d+)$/);
    if (versionMatch) {
      const [, major, minor] = versionMatch;
      const hit = await findFirstExistingCandidate(getVersionedEngineRoots(major, minor));
      if (hit) return hit;
    }

    const iniPath = path.join(path.dirname(uprojectFile), 'Config', 'DefaultEngine.ini');
    try {
      const iniContent = await fs.promises.readFile(iniPath, 'utf-8');
      const iniMatch = iniContent.match(/EnginePath\s*=\s*(.+)/);
      if (iniMatch) {
        const iniEnginePath = iniMatch[1].trim().replace(/["']/g, '');
        return await findFirstExistingCandidate([iniEnginePath]);
      }
    } catch (error) {
      if (!(error instanceof Error)) {
        throw error;
      }
    }
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
  }
  return '';
}

export async function findUbtExecutable(): Promise<string> {
  const enginePath = process.env.UE_ENGINE_PATH ?? process.env.UNREAL_ENGINE_PATH ?? undefined;
  if (enginePath) {
    const roots = isEngineDirectoryPath(enginePath)
      ? [enginePath]
      : [path.join(enginePath, 'Engine')];
    const hit = await findFirstExistingCandidate(roots);
    if (hit) return hit;
  }

  const projectPath = process.env.UE_PROJECT_PATH;
  if (projectPath) {
    const uprojectFile = await findProjectFile(projectPath);
    if (uprojectFile) {
      const hit = await findFromProjectAssociation(uprojectFile);
      if (hit) return hit;
    }
  }

  try {
    const whichCmd = process.platform === 'win32' ? 'where' : 'which';
    const { stdout } = await execAsync(`${whichCmd} UnrealBuildTool`, {
      encoding: 'utf-8',
      timeout: 5000,
    });
    const first = stdout.trim().split(/\r?\n/)[0];
    if (first) return first;
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
  }

  return '';
}

export async function findBundledDotNetRoot(ubtPath: string): Promise<string | undefined> {
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
        .sort((a, b) => compareAscii(dotNetVersionKey(b), dotNetVersionKey(a)));

      for (const versionDir of versionDirs) {
        const runtimeRoot = path.join(dotNetBase, versionDir, platformFolder);
        const dotnetExecutable = path.join(runtimeRoot, process.platform === 'win32' ? 'dotnet.exe' : 'dotnet');
        const hit = await tryUbtPath(dotnetExecutable);
        if (hit) {
          return runtimeRoot;
        }
      }
    } catch (error) {
      if (!(error instanceof Error)) {
        throw error;
      }
    }

    const parent = path.dirname(candidateRoot);
    if (parent === candidateRoot) {
      break;
    }
    candidateRoot = parent;
  }

  return undefined;
}
