// Wiring contracts of subdivide's schemes: catmull_clark, loop and bilinear drive the engine's FSubdividePoly, which needs
// an editor and the engine's modeling module to run. Behaviour itself belongs to the integration cases in
// tests/mcp-tools/world/manage-geometry-subdivision.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));
const readModule = (...segments: readonly string[]): string => strip(readFileSync(join(MODULE, ...segments), 'utf8'));

describe('subdivide schemes drive the engine\'s FSubdividePoly over a polygroup cage', () => {
  const schemes = (): string => read('Mesh', 'McpAutomationBridge_GeometrySubdivideSchemes.cpp');

  it('maps catmull_clark, loop and bilinear to their FSubdividePoly schemes', () => {
    expect(schemes()).toContain(
      'Subdivider.SubdivisionScheme = bLoop ? ESubdivisionScheme::Loop : (Scheme == TEXT("bilinear") ? ESubdivisionScheme::Bilinear : ESubdivisionScheme::CatmullClark);');
    expect(schemes()).toContain('FSubdividePoly Subdivider(Topology, Source, Level);');
    expect(schemes()).toMatch(/Subdivider\.ComputeTopologySubdivision\(\) && Subdivider\.ComputeSubdividedMesh\(Subdivided\)/u);
  });

  it('subdivide sends every scheme but pn here and keeps pn on the PN tessellation', () => {
    const reduction = read('Mesh', 'McpAutomationBridge_GeometryMeshReduction.cpp');
    expect(reduction).toMatch(/if \(Scheme != TEXT\("pn"\)\)\s*\{\s*return SubdivideByScheme\(Self, RequestId, Socket, \*Target, ActorName, Scheme, Iterations\);/u);
    expect(reduction).toContain('ApplyPNTessellation(Mesh, FGeometryScriptPNTessellateOptions(), 1, nullptr)');
    expect(reduction).toContain('Scheme != TEXT("catmull_clark") && Scheme != TEXT("loop") && Scheme != TEXT("bilinear")');
    expect(reduction).toContain('Result->SetStringField(TEXT("scheme"), TEXT("pn"));');
  });

  it('reads polygroups as the cage through FGroupTopology, with every boundary vertex a corner', () => {
    expect(schemes()).toContain('UE::Geometry::FGroupTopology Topology(&Source, false);');
    expect(schemes()).toMatch(/Topology\.ShouldAddExtraCornerAtVert = \[&Source\]\([^)]*\)\s*\{\s*return Source\.IsBoundaryVertex\(VertexId\);\s*\};/u);
    expect(schemes()).toContain('Check.ValidateTopology()');
    expect(schemes()).toContain('Group.Boundaries[0].GroupEdges.Num()');
  });

  it('never hands FGroupTopology a mesh without polygroups or with negative or huge ids', () => {
    const source = schemes();
    const guard = source.indexOf('Source.HasTriangleGroups() && Source.MaxGroupID() <= SubdivMaxPolygroupId');
    const negative = source.indexOf('Source.GetTriangleGroup(TriangleId) >= 0');
    const rebuild = source.indexOf('Topology.RebuildTopology()');
    expect(guard).toBeGreaterThan(-1);
    expect(negative).toBeGreaterThan(guard);
    expect(rebuild).toBeGreaterThan(negative);
    expect(source).toMatch(/if \(bGroupsUsable\)\s*\{[\s\S]*?Topology\.RebuildTopology\(\)/u);
    // Loop copies each triangle's group to its children, so a group-less mesh gets groups first.
    expect(source).toMatch(/if \(bLoop && !Source\.HasTriangleGroups\(\)\)\s*\{\s*Source\.EnableTriangleGroups\(\);/u);
  });

  it('refuses a mesh that is no cage and says how to build one', () => {
    const source = schemes();
    expect(source).toContain('TEXT("INVALID_CAGE")');
    expect(source).toContain('edit_dynamic_mesh append_polygons');
    expect(source).toContain('create_box');
    expect(source).toContain('scheme loop subdivides the triangles as they are and needs no cage');
    for (const outcome of ['NoGroups', 'InsufficientGroups', 'UnboundedPolygroup', 'MultiBoundaryPolygroup']) {
      expect(source, outcome).toContain(`FSubdividePoly::ETopologyCheckResult::${outcome}`);
    }
  });

  it('checks the triangle budget from the cage before refining, and changes nothing on failure', () => {
    const source = schemes();
    const guard = source.indexOf('GuardMeshBudget(Self, RequestId, Socket, EstimatedTriangles, TEXT("Subdivide"))');
    const refine = source.indexOf('Subdivider.ComputeTopologySubdivision()');
    const swap = source.indexOf('Target.Mesh->SetMesh(MoveTemp(Subdivided));');
    expect(guard).toBeGreaterThan(-1);
    expect(refine).toBeGreaterThan(guard);
    expect(swap).toBeGreaterThan(refine);
    expect(source).toContain('EstimatedTriangles = (Corners * 2) << (2 * (Level - 1));');
    expect(source).toContain('Target.Mesh->ProcessMesh([&Source](const UE::Geometry::FDynamicMesh3& Original) { Source = Original; });');
    // An engine without OpenSubdiv hands the mesh back unrefined; that is a failure, not a success.
    expect(source).toContain('Subdivided.TriangleCount() <= TrianglesBefore');
    expect(source).toContain('TEXT("SUBDIVISION_FAILED")');
  });

  it('never reads a UV layer the mesh does not have (the engine did so unchecked before 5.3)', () => {
    expect(schemes()).toContain('Source.Attributes()->NumUVLayers() > 0 && Source.Attributes()->GetUVLayer(0)->ElementCount() > 0');
    expect(schemes()).toContain('Subdivider.UVComputationMethod = bUseUvs ? ESubdivisionOutputUVs::Interpolated : ESubdivisionOutputUVs::None;');
  });

  it('replies with the scheme, level, both triangle counts and the cage face count', () => {
    const source = schemes();
    for (const field of ['scheme', 'level', 'trianglesBefore', 'trianglesAfter', 'cageFaces']) {
      expect(source, field).toContain(`TEXT("${field}")`);
    }
  });

  it('the module is optional: a compile guard, a runtime check, and a clear refusal', () => {
    const source = schemes();
    expect(source).toContain('#if MCP_HAS_SUBDIVIDE_POLY');
    expect(source).toContain('#include "Operations/SubdividePoly.h"');
    expect(source).toContain('Modules.IsModuleLoaded(TEXT("ModelingComponentsEditorOnly"))');
    expect(source).toContain('Modules.LoadModule(TEXT("ModelingComponentsEditorOnly"))');
    expect(source.match(/TEXT\("SUBDIVISION_UNAVAILABLE"\)/gu)).toHaveLength(2);

    const build = readModule('McpAutomationBridge.Build.cs');
    expect(build).toContain('AddOptionalModule(Target, EngineDir, "ModelingComponentsEditorOnly", "ModelingComponentsEditorOnly", true)');
    expect(build).toContain('"MCP_HAS_SUBDIVIDE_POLY=1" : "MCP_HAS_SUBDIVIDE_POLY=0"');

    const descriptor = JSON.parse(readFileSync(join('plugins', 'McpAutomationBridge', 'McpAutomationBridge.uplugin'), 'utf8')) as { Plugins: Array<{ Name: string; Optional?: boolean }> };
    expect(descriptor.Plugins.find((plugin) => plugin.Name === 'MeshModelingToolset')).toMatchObject({ Optional: true });
  });
});
