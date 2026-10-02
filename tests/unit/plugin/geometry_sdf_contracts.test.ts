/**
 * Source contracts for create_sdf: shapes blended as one signed distance field and meshed by
 * the engine's marching cubes. The C++ cannot run here, so these pin the rules the reply and
 * the record promise: the field sign the engine expects, the order the blends apply in, which
 * shape owns each triangle, and the refusals.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';

const geometry = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Geometry');
/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (file: string): string =>
  readFileSync(resolve(geometry, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('create_sdf meshes one field', () => {
  const handler = code('Primitives/McpAutomationBridge_GeometryPrimitivesImplicit.cpp');
  const field = code('Primitives/McpAutomationBridge_GeometrySdfField.h');

  it('is dispatched from the geometry handler table', () => {
    expect(code('McpAutomationBridge_GeometryHandlers.cpp')).toContain('SubAction == TEXT("create_sdf")) return HandleCreateSdf(');
  });

  it('hands marching cubes the negated field at iso 0, the sign Implicit/Morphology.h uses', () => {
    expect(handler).toContain('return -McpGeometrySdf::FieldDistance(Shapes, Pt);');
    expect(handler).toContain('Cubes.IsoValue = 0.0;');
    expect(handler).toContain('ERootfindingModes::Bisection');
  });

  it('applies each shape onto the ones before it with a smooth union, subtract or intersect', () => {
    expect(field).toMatch(/double D = LocalDistance\(Shapes\[0\], Pt\);\s*int32 Owner = 0;\s*for \(int32 Index = 1;/u);
    expect(field).toContain('D = FMath::Lerp(D, Ds, H) - K * H * (1.0 - H);');
    expect(field).toContain('D = FMath::Lerp(D, -Ds, H) + K * H * (1.0 - H);');
    expect(field).toContain('D = FMath::Lerp(Ds, D, H) + K * H * (1.0 - H);');
  });

  it('bounds the grid by the unions only, so a cutter never widens it', () => {
    // Reach is BoundRadius, computed once per shape when the shapes are read (repeat and mirror copies keep it).
    expect(handler).toMatch(/if \(S\.Op == EOp::Union\)\s*\{\s*const double R = S\.Reach \+ S\.Blend;/u);
    expect(handler).toContain('S.Reach = McpGeometrySdf::BoundRadius(S);');
  });

  it('gives each triangle to the shape whose field decides the surface there: polygroup index+1 and that slot', () => {
    // A union owns where it is the nearer surface, a subtract where its carve wins, an intersect where it is the
    // tighter bound: the switch sits on the fillet's centre line. "Nearest surface" painted a thin band blotchy and
    // left a cutter's slot colour smeared over the faces around it.
    expect(field).toContain('if (Ds < D) Owner = Index;');
    expect(field).toContain('if (-Ds > D) Owner = Index;');
    expect(field).toContain('if (Ds > D) Owner = Index;');
    expect(handler).toContain('McpGeometrySdf::FieldOwner(Shapes, Mesh.GetTriCentroid(Tid))');
    expect(handler).not.toContain('FMath::Abs(McpGeometrySdf::LocalDistance');
    // A repeat or mirror copy counts as the shapes[] entry that made it.
    expect(handler).toContain('Mesh.SetTriangleGroup(Tid, Owner.Source + 1);');
    expect(handler).toContain('MaterialIds->SetValue(Tid, Owner.MaterialId);');
  });

  it('splits the edges where ownership changes first, so a part boundary is a smooth line, not triangle steps', () => {
    expect(handler).toMatch(/void AssignParts\([^)]*\)\s*\{\s*SplitAtPartBoundaries\(Mesh, Shapes\);/u);
    expect(handler).toContain('Mesh.SplitEdge(Eid, Info, 0.5 * (Lo + Hi));');
  });

  it('keeps resolution in range and refuses what it cannot mesh', () => {
    expect(handler).toContain('FMath::Clamp(GetJsonIntField(Payload, TEXT("resolution"), 128), 16, 256)');
    expect(handler).toContain('its operation must be union');
    expect(handler).toContain('TEXT("SDF_EMPTY")');
  });

  it('holds the mesh it made to the triangle cap, so the documented 256 is reachable', () => {
    // 8 * N^2 refused every resolution-256 call (524,288 > 500,000) whatever the shapes were.
    expect(handler).toContain('GuardMeshBudget(Self, RequestId, Socket, Mesh.TriangleCount(), TEXT("SDF meshing at this resolution"))');
    expect(handler).not.toContain('8LL * Resolution * Resolution');
  });
});

describe('create_primitive offers sdf', () => {
  it('lists the variant and declares the shapes and resolution the handler reads, and the parts it returns', () => {
    const record = capabilityIndex().byId.get('manage_geometry.create_primitive');
    const input = record?.schemas.input.properties as Record<string, { enum?: unknown[] }> | undefined;
    expect(input?.primitive?.enum).toContain('sdf');
    expect(Object.keys(input ?? {})).toEqual(expect.arrayContaining(['shapes', 'resolution']));
    const output = record?.schemas.output.properties as Record<string, unknown> | undefined;
    expect(Object.keys(output ?? {})).toEqual(expect.arrayContaining(['parts', 'vertexCount', 'triangleCount']));
  });
});
