// Wiring contracts of edit_dynamic_mesh append_polygons that no unit test can reach by running it (it needs an editor):
// everything is validated before the mesh is touched, and the cage is appended under one edit. Behaviour itself belongs
// to the integration cases in tests/mcp-tools/world/manage-geometry-polygons.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('append_polygons validates everything before it touches the mesh', () => {
  const input = (): string => read('Modeling', 'McpAutomationBridge_GeometryPolygonInput.cpp');
  const handler = (): string => read('Modeling', 'McpAutomationBridge_GeometryAppendPolygons.cpp');

  it('caps the call, checks indices and finite coordinates, and names the face at fault', () => {
    const source = input();
    expect(source).toContain('static constexpr int32 MaxAppendVertices = 20000;');
    expect(source).toContain('static constexpr int32 MaxAppendFaces = 20000;');
    expect(source).toContain('!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)');
    expect(source).toContain('faces[%d] uses vertex index %.0f, but vertices has %d points');
    expect(source).toContain('faces[%d] uses vertex %.0f twice');
    expect(source).toContain('Corners->Num() < 3 || Corners->Num() > MaxAppendFaceCorners');
  });

  it('ear clips faces of more than three corners in the winding they were given', () => {
    // The block comment naming bOrientAsHoleFill is stripped before this reads, leaving only `false`: same winding, not a hole fill.
    expect(input()).toMatch(/PolygonTriangulation::TriangulateSimplePolygon\(Polygon, Local,\s+false\);/u);
    expect(input()).toContain('Local.Num() != Face.Num() - 2');
  });

  it('refuses inconsistent winding and edges shared by more than two faces', () => {
    const source = input();
    expect(source).toContain('both run from vertex %u to vertex %u');
    expect(source).toContain('if (++Count > 2)');
  });

  it('appends under one edit, one unique polygroup per face unless faceGroups is given, and restores on a refusal', () => {
    const source = handler();
    expect(source).toContain('Input.FaceGroups.Num() > 0 ? Input.FaceGroups[FaceIndex] : EditMesh.AllocateTriangleGroup()');
    expect(source).toContain('MaterialIds->SetValue(TriangleId, Input.FaceMaterials[Face]);');
    expect(source).toContain('Mesh->SetMesh(MoveTemp(Backup));');
    expect(source).toContain('GuardMeshBudget(Self, RequestId, Socket,');
    for (const field of ['verticesAdded', 'facesAdded', 'trianglesAdded', 'vertexCount', 'triangleCount', 'groupCount']) {
      expect(source, field).toContain(`TEXT("${field}")`);
    }
  });

  it('keeps polygroup ids small enough for FGroupTopology to size its arrays by them', () => {
    expect(input()).toContain('static constexpr int32 MaxAppendGroupId = 1000000;');
    expect(input()).toContain('ParseAppendFaceIntegers(Payload, TEXT("faceGroups"), OutInput.Faces.Num(), MaxAppendGroupId');
  });

  it('leaves out a point no face uses, so no isolated vertex stretches the mesh bounds', () => {
    const source = read('Modeling', 'McpAutomationBridge_GeometryAppendPolygons.cpp');
    expect(source).toContain('Used[Triangle.A] = Used[Triangle.B] = Used[Triangle.C] = true;');
    expect(source).toContain('if (Used[VertexIndex])');
    expect(source).toContain('Result->SetNumberField(TEXT("verticesAdded"), VerticesAdded);');
  });

  it('the dispatcher routes append_polygons to HandleAppendPolygons', () => {
    expect(read('McpAutomationBridge_GeometryHandlers.cpp')).toContain('if (SubAction == TEXT("append_polygons")) return HandleAppendPolygons(this, RequestId, Payload, RequestingSocket);');
  });
});
