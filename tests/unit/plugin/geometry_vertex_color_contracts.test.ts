// Wiring contracts of bake_vertex_colors: where the masks are written, what each channel holds, and how occlusion is
// traced, none of which a unit test can run without an editor. Behaviour itself belongs to the integration cases in
// tests/mcp-tools/world/manage-geometry-vertex-colors.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge');
const GEOMETRY = join(MODULE, 'Private', 'Domains', 'Geometry');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(GEOMETRY, ...segments), 'utf8'));

describe('bake_vertex_colors writes masks the material reads, white meaning no effect', () => {
  const bake = (): string => read('Mesh', 'McpAutomationBridge_GeometryVertexColorBake.cpp');

  it('writes the primary colour overlay, the store the static-mesh conversion reads', () => {
    const source = bake();
    expect(source).toContain('EditMesh.Attributes()->EnablePrimaryColors();');
    expect(source).toContain('UE::Geometry::FDynamicMeshColorOverlay* Overlay = EditMesh.Attributes()->PrimaryColors();');
    expect(source).toContain('Overlay->AppendElement(Colors[VertexId])');
    expect(source).toContain('Overlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(');
    // The legacy per-vertex store is not converted to a static mesh, so it must not be the one written.
    expect(source).not.toContain('EnableVertexColors');
    expect(source).not.toContain('SetVertexColor(');
  });

  it('R is openness, G and B are one minus the edge and cavity masks, A is height in local Z', () => {
    const source = bake();
    expect(source).toContain('Masks[VertexId] = FVector3f(Openness[VertexId],');
    expect(source).toContain('1.0f - FMath::Clamp(static_cast<float>(Convex[VertexId] * PerRadian), 0.0f, 1.0f)');
    expect(source).toContain('1.0f - FMath::Clamp(static_cast<float>(Concave[VertexId] * PerRadian), 0.0f, 1.0f)');
    expect(source).toContain('Colors[VertexId] = FVector4f(Masks[VertexId].X, Masks[VertexId].Y, Masks[VertexId].Z, Height);');
    expect(source).toContain('(Source.GetVertex(VertexId).Z - Bottom) / Span');
    expect(source).toContain('OutOpenness.Init(1.0f, Mesh.MaxVertexID());');
  });

  it('writes the masks as they are: no sRGB transform, because the conversion cancels the build\'s encoding', () => {
    const source = bake();
    expect(source).not.toMatch(/SRGB|ToFColor|Pow\(/iu);
  });

  it('traces occlusion with hemisphere rays against the mesh\'s own AABB tree, in parallel', () => {
    const source = bake();
    expect(source).toContain('UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);');
    expect(source).toContain('Tree.TestAnyHitTriangle(FRay3d(Origin,');
    expect(source).toContain('Options.MaxDistance = Distance;');
    expect(source).toContain('ParallelFor(VertexIds.Num(),');
    expect(source).toContain('static constexpr int32 MinAoRays = 8;');
    expect(source).toContain('static constexpr int32 MaxAoRays = 256;');
    expect(source).toContain('GetJsonIntField(Payload, TEXT("aoRays"), DefaultAoRays)');
    expect(source).toContain('(Bounds.Max - Bounds.Min).Length() * 0.15');
  });

  it('classifies a fold as convex when the far corner of the second triangle sits behind the first', () => {
    const source = bake();
    expect(source).toContain('NormalA.Dot(Mesh.GetVertex(Far.B) - Mesh.GetVertex(Ends.A)) < 0.0');
    expect(source).toContain('TArray<float>& Folds = bConvex ? OutConvex : OutConcave;');
    expect(source).toContain('CurvatureScale * 2.0 / BakePi');
  });

  it('blurs the three masks over one-ring neighbours, blurIterations times', () => {
    expect(bake()).toContain('Mesh.VtxVerticesItr(VertexId)');
    expect(bake()).toContain('BakeBlurMasks(Source, Blur, Masks);');
    expect(bake()).toContain('GetJsonIntField(Payload, TEXT("blurIterations"), 1), 0, MaxBakeBlurIterations');
  });

  it('the dispatcher routes bake_vertex_colors to HandleBakeVertexColors', () => {
    expect(read('McpAutomationBridge_GeometryHandlers.cpp')).toContain('if (SubAction == TEXT("bake_vertex_colors")) return HandleBakeVertexColors(this, RequestId, Payload, RequestingSocket);');
  });
});
