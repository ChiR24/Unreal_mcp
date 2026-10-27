// Wiring contracts of build_environment erode_landscape that no unit test can reach by running it (it needs an editor and
// a landscape). The erosion itself is pinned by the in-editor known answers (EngineWire.Landscape.TerrainErosion) and the
// integration cases in tests/mcp-tools/world/erode-landscape.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const LANDSCAPE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Landscape');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(LANDSCAPE, ...segments), 'utf8'));

describe('erode_landscape', () => {
  const handler = read('Erosion', 'McpLandscapeErode.cpp');
  const shape = read('Erosion', 'McpTerrainShape.cpp');

  // (h + F h_r) / (1 + F) rounds a cell level with its receiver to an ulp below it: every filled lake became pits.
  it('steps toward the receiver in the form that keeps a level cell level', () => {
    expect(shape).toContain('Grid.Heights[Cell] = Grid.Heights[Receiver] + (Grid.Heights[Cell] - Grid.Heights[Receiver]) / (1.0 + F);');
    expect(shape).toContain('if (Receiver == INDEX_NONE || Grid.Heights[Cell] <= Grid.Heights[Receiver])');
  });

  it('lays down what a droplet still carries when it runs dry or out of time', () => {
    expect(shape).toMatch(/if \(!bLeft && Sediment > 0\.0\)\s*\{\s*Drop\(X, Y, Sediment\);/u);
  });

  // With no layer selected the edit interface reads the merged surface: a dryRun would answer for other heights.
  it('reads the edit layer the write goes to, and puts the selection back', () => {
    const select = handler.indexOf('Landscape->SetEditingLayer(McpLandscapeHandlers::LandscapeWriteLayer(Landscape));');
    const readAt = handler.indexOf('Read.GetHeightData(');
    const restore = handler.indexOf('Landscape->SetEditingLayer(Selected);');
    expect(select).toBeGreaterThan(0);
    expect(select).toBeLessThan(readAt);
    expect(readAt).toBeLessThan(restore);
    expect(read('McpLandscapeLookup.cpp')).toContain('Landscape->SetEditingLayer(LandscapeWriteLayer(Landscape));');
  });

  it('judges drainage on the rounded samples it writes, and writes in one undo step', () => {
    expect(handler.indexOf('Raw[Index] = ErodeToRaw(Grid.Heights[Index], Scale.Z);')).toBeLessThan(handler.indexOf('McpTerrainErosion::Undrained(Grid)'));
    expect(handler).toContain('const McpLandscapeHandlers::FMcpLandscapeEditScope Edit(Landscape, TEXT("Erode Landscape"), false);');
    expect(handler).toMatch(/if \(!bDryRun && Changed > 0\)/u);
  });

  it('places river points through the landscape actor\'s whole transform', () => {
    expect(handler).toContain('const FTransform ToWorld = Landscape->GetActorTransform();');
    expect(handler).toContain('ToWorld.TransformPosition(FVector(MinX + Point.X, MinY + Point.Y, Point.Z / ScaleZ))');
  });

  it('caps a call at the vertex count the record names', () => {
    expect(handler).toContain('constexpr int64 ErodeMaxCells = 4194304;');
    const record = readFileSync(join('src', 'tools', 'catalog', 'capabilities', 'records', 'build-environment', 'landscape.data.ts'), 'utf8');
    expect(record).toContain('more than 4,194,304 vertices at once: name a region');
  });

  it('the dispatcher routes erode_landscape', () => {
    expect(strip(readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'McpEnvHandlers.cpp'), 'utf8')))
      .toMatch(/LowerSub == TEXT\("erode_landscape"\)\)\s*\{\s*return McpLandscapeErosion::HandleErodeLandscape\(\*this, RequestId, Payload, RequestingSocket\);/u);
  });
});
