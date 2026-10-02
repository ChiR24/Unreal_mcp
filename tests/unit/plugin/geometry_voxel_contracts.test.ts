// Wiring contracts of morphology and remesh_voxel, the two grid operations: they call the engine's voxel functions and
// put the mesh back when the result is unusable, which needs an editor to see. Behaviour itself belongs to the
// integration cases in tests/mcp-tools/world/manage-geometry-voxel.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('morphology and remesh_voxel are real grid operations that put the mesh back when they fail', () => {
  const voxel = (): string => read('Mesh', 'McpAutomationBridge_GeometryVoxelOperations.cpp');

  it('morphology runs ApplyMeshMorphology on a grid of voxelCount cells along the longest side', () => {
    const source = voxel();
    expect(source).toContain('UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshMorphology(Target->Mesh, Options, nullptr);');
    expect(source).toContain('Options.SDFGridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridResolution;');
    expect(source).toContain('Options.SDFGridParameters.GridResolution = VoxelCount;');
    expect(source).toContain('Options.Operation = OpType;');
    for (const [name, type] of [['dilate', 'Dilate'], ['contract', 'Contract'], ['open', 'Open']] as const) {
      expect(source).toContain(`Operation == TEXT("${name}")) OpType = EGeometryScriptMorphologicalOpType::${type};`);
    }
    expect(source).toContain('GetJsonStringField(Payload, TEXT("operation"), TEXT("close")).ToLower()');
    expect(source).toContain('static constexpr int32 MaxVoxelCount = 256;');
  });

  it('remesh_voxel is the engine\'s voxel wrap, not the uniform remesh plus a hole fill', () => {
    const source = voxel();
    expect(source).toContain('UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(Target->Mesh, Options, nullptr);');
    expect(source).toContain('Options.GridParameters.GridCellSize = static_cast<float>(CellSize);');
    const reduction = read('Mesh', 'McpAutomationBridge_GeometryMeshReduction.cpp');
    expect(reduction).not.toContain('FillAllMeshHoles');
    expect(reduction).not.toContain('bVoxel');
  });

  it('maps targetEdgeLength, then voxelCount, then a triangle budget onto the grid cell', () => {
    const source = voxel();
    const edge = source.indexOf('Payload->HasField(TEXT("targetEdgeLength"))');
    const count = source.indexOf('Payload->HasField(TEXT("voxelCount"))');
    const budget = source.indexOf('Payload->HasField(TEXT("targetTriangleCount"))');
    expect(edge).toBeGreaterThan(-1);
    expect(count).toBeGreaterThan(edge);
    expect(budget).toBeGreaterThan(count);
    expect(source).toContain('CellSize = FMath::Max(CellSize, Longest / MaxVoxelCount);');
  });

  it('restores the mesh when a result is empty or over the triangle cap', () => {
    const source = voxel();
    expect(source).toContain('Target.Mesh->SetMesh(MoveTemp(Backup));');
    expect(source).toContain('After > 0 && After <= MAX_TRIANGLES_PER_DYNAMIC_MESH');
    expect(source.match(/KeepVoxelResult\(Self, RequestId, Socket, \*Target, Backup,/gu)).toHaveLength(2);
    expect(source).toContain('TEXT("OPERATION_EMPTY")');
  });

  it('says in its reply that the surface was rebuilt and what that drops', () => {
    expect(voxel()).toContain('UVs, material ids, polygroups and vertex colours are gone');
    expect(voxel().match(/TEXT\("note"\)/gu)).toHaveLength(2);
  });

  it('the dispatcher routes morphology to HandleMorphology', () => {
    expect(read('McpAutomationBridge_GeometryHandlers.cpp')).toContain('if (SubAction == TEXT("morphology")) return HandleMorphology(this, RequestId, Payload, RequestingSocket);');
  });

  it('the dispatcher routes remesh_voxel to HandleRemeshVoxel', () => {
    expect(read('McpAutomationBridge_GeometryHandlers.cpp')).toContain('if (SubAction == TEXT("remesh_voxel")) return HandleRemeshVoxel(this, RequestId, Payload, RequestingSocket);');
  });
});
