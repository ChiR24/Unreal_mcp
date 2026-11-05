// Wiring contracts of system_control list_output_files / delete_output_file: which folders they reach, and the
// folders a delete leaves empty. Behaviour belongs to the integration cases in tests/mcp-tools/core/system-control.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const FILE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'SystemControl', 'McpSystemOutputFiles.cpp');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (): string => readFileSync(FILE, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('output files', () => {
  // Contact sheets and capture passes go to Saved/Captures and EngineWire keeps its own files under Saved/EngineWire
  // (layout images, the asset index, recipes, plugins remove_plugin moved aside): neither could be listed or deleted.
  // export_mesh writes to Saved/Exports by default and the thumbnail action's outputPath points at Saved/Thumbnails:
  // files there could not be removed without reaching into the project from a shell.
  it('reach the captures, EngineWire, exports and thumbnails folders beside the screenshot and snapshot folders', () => {
    expect(code()).toMatch(/OutputRoots\[\] = \{TEXT\("Saved\/Screenshots"\), TEXT\("Saved\/Captures"\), TEXT\("Saved\/EngineWire"\),\s*TEXT\("Saved\/Exports"\), TEXT\("Saved\/Thumbnails"\), TEXT\("Saved\/unreal-mcp"\),\s*TEXT\("tmp\/unreal-mcp"\), TEXT\("temp\/unreal-mcp"\)\};/u);
  });

  it('a delete takes the folders it leaves empty, never the root and never a folder still holding something', () => {
    const source = code();
    expect(source).toMatch(/for \(FString Dir = FPaths::GetPath\(Full\); !bExistsAfter && Dir\.Len\(\) > RootFull\.Len\(\) &&\s*IFileManager::Get\(\)\.DeleteDirectory\(\*Dir, false, false\);/u);
    expect(source).not.toMatch(/DeleteDirectory\([^)]*true\)/u);
  });
});
