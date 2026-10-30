// create_morph_target and set_morph_target_deltas write the morph into the mesh's source data. A morph registered from
// render data alone is dropped by the next rebuild from the source data, and from UE 5.4 RegisterMorphTarget rebuilds
// at once, so the reply said created while the mesh kept no morph. Behaviour belongs to the integration cases in
// tests/mcp-tools/gameplay/animation-physics.test.mjs, which read the morph back and import it into another mesh.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SKELETON = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Skeleton');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...parts: string[]): string =>
  readFileSync(join(SKELETON, ...parts), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('morph targets made from deltas last', () => {
  const editing = read('MorphTargets', 'McpSkeletonMorphTargetEditing.cpp');
  const source = editing.slice(editing.indexOf('bool WriteSourceMorph('), editing.indexOf('bool RegisterRenderMorph('));

  it('writes the morph into the source data and reads it back after the rebuild', () => {
    expect(source).toContain('McpSkinSource::Write(Mesh, LOD, Source, false, {Name}, OutError)');
    expect(source.indexOf('McpSkinSource::Write(')).toBeLessThan(source.indexOf('Mesh->FindMorphTarget(Name)'));
    expect(source).toContain('TEXT("MORPH_NOT_BUILT")');
    // The source data is read first; a render-data morph only where the mesh has none.
    expect(editing).toMatch(/if \(McpSkinSource::Read\(Mesh, LOD, true, Source, Code, Error\)\)\s*\{\s*bApplied = WriteSourceMorph\(/u);
    expect(editing).toMatch(/else if \(Code == TEXT\("SOURCE_DATA_MISSING"\)\)\s*\{\s*bApplied = RegisterRenderMorph\(/u);
  });

  it('moves every source vertex at a render vertex\'s place, and refuses before anything is written', () => {
    expect(source).toContain('Grid.FindAll(Vertices[Vertex].Position, Tolerance, Matches);');
    const write = source.indexOf('McpSkinSource::Write(');
    for (const code of ['INVALID_ARGUMENT', 'INVALID_MORPH_DATA', 'RENDER_ONLY_MORPHS']) {
      expect(source.indexOf(`TEXT("${code}")`), code).toBeGreaterThan(-1);
      expect(source.indexOf(`TEXT("${code}")`), code).toBeLessThan(write);
    }
  });

  it('checks a render-data morph on a throwaway object before the mesh is touched', () => {
    const render = editing.slice(editing.indexOf('bool RegisterRenderMorph('), editing.indexOf('bool ApplyMorph('));
    expect(render.indexOf('NewObject<UMorphTarget>(GetTransientPackage())')).toBeLessThan(render.indexOf('Mesh->Modify();'));
    expect(render.indexOf('TEXT("INVALID_MORPH_DATA")')).toBeLessThan(render.indexOf('Mesh->Modify();'));
    expect(render.indexOf('Mesh->RegisterMorphTarget(Morph);')).toBeLessThan(render.indexOf('if (!Mesh->FindMorphTarget(Name))'));
  });

  it('finds every point of the source grid within a radius', () => {
    expect(read('SkinWeights', 'McpSkeletonSkinWeightSource.h')).toMatch(/void FindAll\(const FVector3f& Query, float Radius, TArray<int32>& Out\) const\s*\{\s*Grid\.FindPointsInBall\(/u);
  });
});
