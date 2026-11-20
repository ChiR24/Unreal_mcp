// A capability whose handler some builds compile out lists the features it needs (availability.compiled); each one is
// registered with MCP_COMPILED_FEATURE inside the compile guard the handler builds under, so a build that left it out
// (an older engine, or a package without the plugin it is built on) reads it unavailable in search and describe, and
// execute refuses it FEATURE_UNAVAILABLE (EngineWire.Gateway.CompiledGate runs that in the editor). A handler that
// reaches the engine only by reflection lists the engine roles it needs instead (availability.roles), judged the same
// way where nothing plays one on the running engine (EngineWire.Gateway.RoleGate). No record names an engine version.

import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sourceShape } from '../../../src/tools/catalog/capabilities/record-fields.js';
import { capabilityIndex } from '../support/capability-index.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const GATEWAY = join(PRIVATE, 'MCP');
const read = (...segments: readonly string[]): string => readFileSync(join(GATEWAY, ...segments), 'utf8');

describe('availability.compiled', () => {
  it('marks the capabilities a build may leave out, by the guard each handler compiles under', () => {
    const index = capabilityIndex();
    const compiled = (id: string): readonly string[] | undefined => index.byId.get(id)?.availability.compiled;
    expect(compiled('animation_physics.setup_retargeting')).toEqual(['IKRIG_BATCH_RETARGET']);
    expect(compiled('animation_physics.skin_mesh_to_skeleton')).toEqual(['GEOMETRY_SCRIPT_BONE_WEIGHTS']);
    expect(compiled('manage_audio.get_metasound_graph')).toEqual(['METASOUND_GRAPH_DATA']);
    expect(compiled('system_control.list_game_features')).toEqual(['GAME_FEATURES']);
    expect(compiled('system_control.set_game_feature_active')).toEqual(['GAME_FEATURES']);
    const geometry = index.byParentTool.get('manage_geometry') ?? [];
    expect(geometry.length).toBeGreaterThan(0);
    expect(compiled('manage_geometry.configure_mesh_collision')).toEqual(['FULL_GEOMETRY_SCRIPT', 'GEOMETRY_COLLISION']);
    expect(geometry.filter((record) => record.availability.compiled?.[0] !== 'FULL_GEOMETRY_SCRIPT')).toEqual([]);
  });

  it('names only features a plugin source registers', () => {
    const registered = new Set<string>();
    for (const file of readdirSync(PRIVATE, { recursive: true, encoding: 'utf8' })) {
      if (!/\.(cpp|h)$/u.test(file)) continue;
      for (const match of readFileSync(join(PRIVATE, file), 'utf8').matchAll(/^\s*MCP_COMPILED_FEATURE\((\w+)\)/gmu)) {
        registered.add(match[1] ?? '');
      }
    }
    const named = capabilityIndex().records.flatMap((record) => record.availability.compiled ?? []);
    expect(named.length).toBeGreaterThan(0);
    expect(named.filter((name) => !registered.has(name))).toEqual([]);
  });

  it('is judged with the version gate by every surface that says whether a capability runs', () => {
    const catalog = read('Gateway', 'McpNativeGatewayCatalog.cpp');
    expect(catalog).toContain('return McpMissingCompiledFeature(Record).IsEmpty() &&');
    expect(catalog).toContain('OutCode = TEXT("FEATURE_UNAVAILABLE");');
    expect(catalog).toContain('Reasons.Add(TEXT("not_compiled"));');
  });
});

describe('availability.roles', () => {
  const ROLES = join('plugins', 'EngineWire', 'Resources', 'EngineRoles');
  const tables = readdirSync(ROLES).filter((file) => file.endsWith('.json') && !file.includes('.generated'))
    .map((file) => JSON.parse(readFileSync(join(ROLES, file), 'utf8')) as { domain: string; roles: { id: string; optional?: boolean }[] });

  it('takes role ids or a role table\'s domain, and never an engine version', () => {
    const withRoles = (roles: unknown): boolean =>
      sourceShape.availability.safeParse({ requiredPlugins: [], editorStates: ['edit'], roles }).success;
    expect(withRoles(['MVVM'])).toBe(true);
    expect(withRoles(['MVVM.Binding.Source'])).toBe(true);
    expect(withRoles(['5.4'])).toBe(false);
    expect(withRoles([])).toBe(false);
    expect(sourceShape.availability.safeParse({ requiredPlugins: [], editorStates: ['edit'], minEngineVersion: '5.4' }).success).toBe(false);
  });

  it('names only roles and domains the role tables define', () => {
    const known = new Set(tables.flatMap((table) => [table.domain, ...table.roles.map((role) => role.id)]));
    const named = capabilityIndex().records.flatMap((record) => record.availability.roles ?? []);
    expect(named.length).toBeGreaterThan(0);
    expect(named.filter((name) => !known.has(name))).toEqual([]);
  });

  it('gates the viewmodel records on the MVVM table, which the header check finds whole from 5.4', () => {
    const viewmodels = capabilityIndex().records.filter((record) => record.discovery.family === 'viewmodel');
    expect(viewmodels.map((record) => record.availability.roles)).toEqual(viewmodels.map(() => ['MVVM']));
    const resolved = JSON.parse(readFileSync(join(ROLES, 'resolved.generated.json'), 'utf8')) as Record<string, Record<string, string | null>>;
    const mvvm = tables.find((table) => table.domain === 'MVVM')?.roles.filter((role) => !role.optional) ?? [];
    const whole = (engine: string): boolean => mvvm.every((role) => (resolved[role.id]?.[engine] ?? '?').match(/^[esw]:/u) !== null);
    expect(['5.3', '5.4', '5.8', '5.9'].map(whole)).toEqual([false, true, true, true]);
  });

  it('is judged by every surface that says whether a capability runs', () => {
    expect(read('Gateway', 'McpNativeGatewayCatalog.cpp')).toContain('return McpMissingCompiledFeature(Record).IsEmpty() && McpMissingEngineRole(Record).IsEmpty();');
    expect(read('Execute', 'McpNativeGatewayValidation.cpp')).toContain('McpCheckAvailabilityGate(*Request.Record, GateCode, GateMessage)');
    expect(read('Gateway', 'McpNativeGatewayCatalog.cpp')).toContain('const bool bRunnable = bEnabled && McpRunsOnThisEngine(Record);');
    expect(read('Gateway', 'McpNativeGatewayCatalog.cpp')).toContain('Row->SetBoolField(TEXT("runnable"), bRunnable);');
    expect(read('Gateway', 'McpNativeGatewaySearch.cpp')).toContain('McpRunsOnThisEngine(*Scored[0].Record);');
    expect(read('Gateway', 'McpNativeGatewayDescribe.cpp')).toContain('bAvailable && McpRunsOnThisEngine(Record)');
    expect(read('Gateway', 'McpNativeGatewayDescribeOverview.cpp')).toContain('McpRunsOnThisEngine(*Record)');
  });
});

describe('edits while Live Coding compiles', () => {
  // The engine marks no package dirty while a reload is active: a sublevel's spawns and an interlude's mesh swaps made
  // during a compile read "0 dirty" in save_all and were dropped by the next level load.
  it('refuses every capability that is not a read until the compile ends', () => {
    const catalog = read('Gateway', 'McpNativeGatewayCatalog.cpp');
    expect(catalog).toContain('if (IsReloadActive() && !Record.Effect.Equals(TEXT("read"), ESearchCase::IgnoreCase))');
    expect(catalog).toContain('OutCode = TEXT("LIVE_CODING_ACTIVE");');
    expect(catalog).toContain('#include "Modules/ModuleManager.h"');
  });
});
