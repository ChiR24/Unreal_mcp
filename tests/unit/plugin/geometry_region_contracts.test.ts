// Wiring contracts of the region selection every face operator shares, and of set_material_id, which selects the same
// way: they need an editor to run. Behaviour itself belongs to the integration cases in
// tests/mcp-tools/world/manage-geometry-region.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';
const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('region picks triangles for the face operators and set_material_id', () => {
  const region = (): string => read('Support', 'McpAutomationBridge_GeometryRegion.cpp');
  const selection = (): string => read('Support', 'McpAutomationBridge_GeometrySelection.cpp');

  it('the shared selection resolves a region and refuses it together with triangleIndices', () => {
    const source = selection();
    expect(source).toContain('if (!ResolveRegionTriangles(Mesh, *Region, TriangleIds, OutError, OutCode)) return false;');
    expect(source).toMatch(/if \(bHasIndices && bHasRegion\)\s*\{\s*OutError = TEXT\("give either triangleIndices or region to select triangles, not both"\);\s*return false;/u);
    expect(source).toContain('Self->SendAutomationError(Socket, RequestId, Error, Code);');
  });

  it('every face operator reads its selection through it and reports how many triangles it picked', () => {
    const files: ReadonlyArray<readonly [readonly string[], readonly string[]]> = [
      [['Modeling', 'McpAutomationBridge_GeometryModelingExtrusion.cpp'], ['HandleExtrude', 'HandleInsetOutset', 'HandleBevel']],
      [['Modeling', 'McpAutomationBridge_GeometryModelingOffsets.cpp'], ['HandleOffsetFaces']],
      [['Topology', 'McpAutomationBridge_GeometryTopologyFaces.cpp'], ['HandlePoke']],
    ];
    for (const [path, handlers] of files) {
      const source = read(...path);
      for (const handler of handlers) {
        const body = sliceBetween(source, `bool ${handler}(`, '\nbool ');
        expect(body, `${handler} selects`).toContain('ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload');
        expect(body, `${handler} reports`).toContain('TEXT("trianglesSelected")');
      }
    }
  });

  it('poke takes the ids from the shared selection, so a region pokes the same triangles', () => {
    const body = sliceBetween(read('Topology', 'McpAutomationBridge_GeometryTopologyFaces.cpp'), 'bool HandlePoke(', '\nbool ');
    expect(body).toContain('ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection, &Requested)');
    expect(body).not.toContain('TEXT("triangleIndices")');
  });

  it('a box, a facing direction, polygroup ids and material ids AND together in the mesh\'s local space', () => {
    const source = region();
    expect(source).toContain('const FVector3d Centroid = ReadMesh.GetTriCentroid(TriangleId);');
    expect(source).toContain('ReadMesh.GetTriNormal(TriangleId).Dot(Normal) < MinDot');
    expect(source).toContain('GetJsonNumberField(Region, TEXT("normalAngle"), 30.0)');
    expect(source).toContain('Groups.Contains(bHasGroups ? ReadMesh.GetTriangleGroup(TriangleId) : 0)');
    expect(source).toContain('Materials.Contains(MaterialIds ? MaterialIds->GetValue(TriangleId) : 0)');
    expect(source.match(/\) continue;/gu)?.length ?? 0).toBeGreaterThanOrEqual(3);
  });

  it('a region that matches nothing is REGION_EMPTY and the message gives the mesh\'s local bounds', () => {
    const source = region();
    expect(source).toContain('OutCode = TEXT("REGION_EMPTY");');
    expect(source).toContain('local bounds are min (%.1f, %.1f, %.1f) max (%.1f, %.1f, %.1f)');
    expect(source).toContain('region needs at least one of box, normal, groupIds or materialIds');
  });

  it('set_material_id selects through the same helper and writes the material id attribute', () => {
    const source = read('Modeling', 'McpAutomationBridge_GeometryMaterialIds.cpp');
    expect(source).toContain('ReadTriangleSelection(Self, RequestId, Socket, Mesh, Payload, Selection, bHasSelection, &Selected)');
    expect(source).toContain('EditMesh.Attributes()->EnableMaterialID();');
    expect(source).toContain('MaterialIds->SetValue(TriangleId, MaterialId);');
    expect(source).toMatch(/IdValue > MAX_MATERIAL_ID/u);
  });


  it('the dispatcher routes set_material_id to HandleSetMaterialId', () => {
    expect(read('McpAutomationBridge_GeometryHandlers.cpp')).toContain('if (SubAction == TEXT("set_material_id")) return HandleSetMaterialId(this, RequestId, Payload, RequestingSocket);');
  });
});
