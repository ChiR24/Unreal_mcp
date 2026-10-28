// Data registries (manage_asset list_data_registries, get_data_registry, create_data_registry) are reached by
// reflection only: the DataRegistry plugin is Beta and may be off, and a build that linked it would fail to load where
// it is off and could not ship in a build that leaves Beta plugins out. Nothing here may include its headers.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SOURCE = join('plugins', 'EngineWire', 'Source', 'EngineWire');
const FOLDER = join(SOURCE, 'Private', 'Domains', 'AssetWorkflow', 'DataRegistries');
const read = (...segments: readonly string[]): string => readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n');

describe('data registries', () => {
  it('include no DataRegistry header and link no DataRegistry module', () => {
    for (const file of readdirSync(FOLDER)) {
      expect(read(FOLDER, file)).not.toMatch(/#include\s+"DataRegistry/u);
    }
    expect(read(SOURCE, 'EngineWire.Build.cs')).not.toMatch(/"DataRegistry"/u);
    const plugin = JSON.parse(read('plugins', 'EngineWire', 'EngineWire.uplugin')) as { Plugins: readonly { Name: string }[] };
    expect(plugin.Plugins.some((each) => each.Name === 'DataRegistry')).toBe(false);
  });

  it('refuses every bad request before anything is made', () => {
    const create = read(FOLDER, 'McpDataRegistryCreate.cpp');
    const made = create.indexOf('NewObject<UObject>(CreatePackage(');
    expect(made).toBeGreaterThan(-1);
    for (const code of ['INVALID_ARGUMENT', 'ASSET_EXISTS', 'DATA_REGISTRY_EXISTS', 'DATA_TABLE_NOT_FOUND', 'ROW_STRUCT_MISMATCH']) {
      const refusal = create.indexOf(`TEXT("${code}")`);
      expect(refusal, code).toBeGreaterThan(-1);
      expect(refusal, code).toBeLessThan(made);
    }
  });
});
