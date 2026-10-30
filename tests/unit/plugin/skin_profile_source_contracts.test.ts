// set_vertex_weights writes its skin weight profile into the mesh's source data from UE 5.4. The build resets the LOD
// model's profiles and makes them from the source data, so a profile written into the LOD model alone was wiped by the
// rebuild that followed while the reply said the weights were set. Behaviour belongs to the integration cases in
// tests/mcp-tools/gameplay/animation-physics.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const WEIGHTS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Skeleton', 'SkinWeights');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(WEIGHTS, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('a skin weight profile set by vertex lasts', () => {
  const handler = read('McpSkeletonWeightProfileSet.cpp');

  it('writes the profile into the source data from UE 5.4, listing it on the mesh before the rebuild', () => {
    const source = handler.slice(handler.indexOf('#if ENGINE_MINOR_VERSION >= 4'), handler.indexOf('#else'));
    expect(source).toContain('McpSkinSource::Read(Mesh, LOD, true, Source, OutCode, OutError, Profile)');
    expect(source).toContain('Grid.FindAll(Vertices[Vertex.Vertex].Position, Tolerance, Matches);');
    expect(source.indexOf('Mesh->AddSkinWeightProfile(Info);')).toBeLessThan(source.indexOf('McpSkinSource::Write('));
    expect(source).toContain('McpSkinSource::Write(Mesh, LOD, Source, true, {}, OutError, Profile)');
    expect(source.indexOf('TEXT("RENDER_ONLY_MORPHS")')).toBeLessThan(source.indexOf('McpSkinSource::Write('));
  });

  it('reads the profile back after the build, and refuses bad weights and the mesh\'s own name before anything changes', () => {
    const body = handler.slice(handler.indexOf('bool HandleSetVertexWeightsAction('));
    expect(body).toContain('SkinWeightProfiles.Find(Profile)');
    expect(body).toContain('TEXT("PROFILE_NOT_BUILT")');
    expect(body.indexOf('TEXT("Default")')).toBeLessThan(body.indexOf('WriteProfile('));
    expect(body.indexOf('ParseProfileWeights(')).toBeLessThan(body.indexOf('WriteProfile('));
    const parse = handler.slice(handler.indexOf('bool ParseProfileWeights('), handler.indexOf('#if ENGINE_MINOR_VERSION >= 4'));
    expect(parse).toContain('Bone >= Ref.GetRawBoneNum()');
    expect(parse).toContain('Vertex >= NumVertices');
  });

  it('reads and writes a named profile in the source data, refused before UE 5.4', () => {
    const io = read('McpSkeletonSkinWeightSource.cpp');
    expect(io).toContain('Attributes.GetVertexSkinWeights(bHasProfile ? Profile : NAME_None)');
    expect(io).toMatch(/Attributes\.RegisterSkinWeightAttribute\(Profile\)[\s\S]*?FSkinWeightsVertexAttributesRef SkinWeights = Attributes\.GetVertexSkinWeights\(Profile\);/u);
    expect(io).toContain('OutCode = TEXT("PROFILE_UNSUPPORTED");');
  });
});
