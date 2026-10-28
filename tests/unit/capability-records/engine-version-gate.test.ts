// A capability whose handler older 5.x builds compile out declares the first engine that has it
// (availability.minEngineVersion). The plugin judges it against the engine it runs in: search and describe read it
// unavailable on an older engine and execute refuses it there (EngineWire.Gateway.EngineGate runs that in the editor).

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sourceShape } from '../../../src/tools/catalog/capabilities/record-fields.js';
import { capabilityIndex } from '../support/capability-index.js';

const GATEWAY = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP');
const read = (...segments: readonly string[]): string => readFileSync(join(GATEWAY, ...segments), 'utf8');

describe('minEngineVersion', () => {
  // Each version is the guard its handler compiles under: MCP_HAS_IKRIG_BATCH_RETARGET (5.8), the skinning calls of
  // MCP_HAS_GEOMETRY_SCRIPT (5.5), MCP_HAS_METASOUND_FRONTEND_V2 (5.5), MCP_HAS_FULL_GEOMETRY_SCRIPT (5.1).
  it('marks the capabilities older engines compile out', () => {
    const index = capabilityIndex();
    const declared = (id: string): string | undefined => index.byId.get(id)?.availability.minEngineVersion;
    expect(declared('animation_physics.setup_retargeting')).toBe('5.8');
    expect(declared('animation_physics.skin_mesh_to_skeleton')).toBe('5.5');
    expect(declared('manage_audio.get_metasound_graph')).toBe('5.5');
    const geometry = index.byParentTool.get('manage_geometry') ?? [];
    expect(geometry.length).toBeGreaterThan(0);
    expect(geometry.filter((record) => record.availability.minEngineVersion !== '5.1')).toEqual([]);
  });

  it('names a later 5.x, never 5.0 (every supported engine) or a malformed version', () => {
    const withVersion = (minEngineVersion: string): boolean =>
      sourceShape.availability.safeParse({ requiredPlugins: [], editorStates: ['edit'], minEngineVersion }).success;
    expect(withVersion('5.4')).toBe(true);
    expect(withVersion('5.0')).toBe(false);
    expect(withVersion('5.4.1')).toBe(false);
    expect(withVersion('5')).toBe(false);
  });

  it('is judged by every surface that says whether a capability runs', () => {
    expect(read('Execute', 'McpNativeGatewayValidation.cpp')).toContain('McpCheckAvailabilityGate(*Request.Record, GateCode, GateMessage)');
    expect(read('Gateway', 'McpNativeGatewayCatalog.cpp')).toContain('Row->SetBoolField(TEXT("runnable"), bEnabled && McpRunsOnThisEngine(Record));');
    expect(read('Gateway', 'McpNativeGatewaySearch.cpp')).toContain('McpRunsOnThisEngine(*Scored[0].Record);');
    expect(read('Gateway', 'McpNativeGatewayDescribe.cpp')).toContain('bAvailable && McpRunsOnThisEngine(Record)');
    expect(read('Gateway', 'McpNativeGatewayDescribeOverview.cpp')).toContain('McpRunsOnThisEngine(*Record)');
  });
});
