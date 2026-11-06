// GeometryScripting stays an Optional plugin reference (it is Experimental on UE 5.0-5.2), so a project can leave it off
// while EngineWire still links its modules. Every GeometryScript call is therefore behind McpGeometryScriptReady():
// manage_geometry checks once in its dispatcher, and each handler elsewhere checks before its first call.

import { readdirSync, readFileSync } from 'node:fs';
import { join, relative } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const GEOMETRY = join(PRIVATE, 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (path: string): string => strip(readFileSync(path, 'utf8'));
const sources = (dir: string): string[] => readdirSync(dir, { withFileTypes: true, recursive: true })
  .filter((entry) => entry.isFile() && /\.(cpp|h)$/u.test(entry.name))
  .map((entry) => join(entry.parentPath, entry.name));

describe('GeometryScript calls refuse while the GeometryScripting plugin is off', () => {
  it('the guard checks the plugin and loads both of its modules, refusing with a code naming the plugin', () => {
    const guard = read(join(GEOMETRY, 'McpGeometryScriptGuard.h'));
    expect(guard).toContain('IPluginManager::Get().FindPlugin(TEXT("GeometryScripting"))');
    expect(guard).toContain('!Plugin.IsValid() || !Plugin->IsEnabled()');
    expect(guard).toContain('{TEXT("GeometryScriptingCore"), TEXT("GeometryScriptingEditor")}');
    expect(guard).toContain('!Modules.IsModuleLoaded(Module) && !(Modules.ModuleExists(Module) && Modules.LoadModule(Module) != nullptr)');
    expect(guard).toContain('TEXT("GEOMETRYSCRIPTING_PLUGIN_NOT_ENABLED")');
    expect(guard).toMatch(/McpGeometryScriptMissing =\s*TEXT\("[^"]*GeometryScripting plugin/u);
  });

  it('manage_geometry refuses before its payload is read or any sub-action is routed', () => {
    const dispatch = read(join(GEOMETRY, 'McpGeometryHandlers.cpp'));
    const guard = dispatch.indexOf('if (!McpGeometryScriptReady())');
    expect(guard).toBeGreaterThan(-1);
    expect(dispatch.slice(guard)).toMatch(/^if \(!McpGeometryScriptReady\(\)\)\s*\{\s*SendAutomationError\(RequestingSocket, RequestId, McpGeometryScriptMissing, McpGeometryScriptMissingCode\);\s*return true;/u);
    for (const later of ['Payload.IsValid()', 'FMcpGeometryUndoScope Undo(Payload)', 'SubAction == TEXT(']) {
      expect(dispatch.indexOf(later), later).toBeGreaterThan(guard);
    }
  });

  it('every file outside Domains/Geometry checks before its first GeometryScript call', () => {
    const callers = sources(PRIVATE).filter((path) => relative(GEOMETRY, path).startsWith('..') && read(path).includes('UGeometryScriptLibrary_'));
    expect(callers.length).toBeGreaterThan(0);
    for (const path of callers) {
      const source = read(path);
      const check = source.indexOf('if (!McpGeometryScriptReady())');
      expect(check, path).toBeGreaterThan(-1);
      expect(check, path).toBeLessThan(source.indexOf('UGeometryScriptLibrary_'));
    }
  });

  it('GeometryScripting stays optional in the descriptor', () => {
    const descriptor = JSON.parse(readFileSync(join('plugins', 'EngineWire', 'EngineWire.uplugin'), 'utf8')) as { Plugins: Array<{ Name: string; Optional?: boolean }> };
    expect(descriptor.Plugins.find((plugin) => plugin.Name === 'GeometryScripting')).toMatchObject({ Optional: true });
  });
});
