// Wiring contracts of build_environment assemble_modules that no unit test can reach by running it (it needs meshes and
// an editor). The face reading and the solver are pinned by the in-editor known answers (EngineWire.Environment.ModuleKit)
// and the integration cases in tests/mcp-tools/world/assemble-modules.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MODULES = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'Modules');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (file: string): string => strip(readFileSync(join(MODULES, file), 'utf8'));

describe('assemble_modules', () => {
  const faces = read('McpModuleFaces.cpp');
  const wave = read('McpModuleWave.h');
  const request = read('McpModuleRequest.cpp');
  const handler = read('McpEnvAssembleModules.cpp');

  // Hamming distance merged a 0.3 and a 0.4 wide corridor; filing faces under socket ids is wrong because the likeness
  // is not transitive.
  it('matches two faces pair by pair, both empty or IoU of at least 0.9', () => {
    expect(faces).toContain('return Either == 0 || 10 * Both >= 9 * Either;');
  });

  // One ray at a square's middle misses a wall 1/40 of the cell thick.
  it('reads each square of a face with 4 x 4 rays from just outside, hits within 3% of the depth', () => {
    expect(faces).toContain('constexpr int32 Sub = 4;');
    expect(faces).toContain('Start[Axis] = Sign * (Half[Axis] + 0.01 * Depth);');
    expect(faces).toContain('Distance <= 0.03 * Depth)');
  });

  // Reading and placing through different transforms would place a turned piece with faces it was never matched by.
  it('reads a piece and places its instance through the same turn', () => {
    expect(request).toContain('SampleFaces(*Trees[Variant.Module], Cell, CellFromMesh(Request.Centre, Variant.Turns), Variant.Faces);');
    expect(handler).toContain('Instances[Variant.Module].Add(CellFromMesh(Request.Centre, Variant.Turns) * FTransform(At));');
  });

  it('turns pieces a quarter turn only in a square cell, and refuses a piece outside its cell box', () => {
    expect(request).toContain('FMath::IsNearlyEqual(Cell.X, Cell.Y) ? TArray<int32>{0, 1, 2, 3} : TArray<int32>{0, 2};');
    expect(request).toContain('does not fit the cell box %s to %s that cellSize and pivot give it');
  });

  // SetNumZeroed keeps the elements already there: a second seed would start from the first one's domains.
  it('starts every seed from fresh domains', () => {
    expect(wave).toContain('Domain.Init(0, Cells * Words);');
    expect(wave).not.toContain('Domain.SetNumZeroed');
  });

  // Reading the faces of 32 pieces at 4 turns casts millions of rays: it has its own time limit, before the solver's.
  it('reads the faces in parallel inside 10 s and lays the kit out inside 20, as one undo step', () => {
    expect(request).toContain('ParallelFor(Request.In.Variants.Num(), [&](int32 Index)');
    expect(handler).toContain('!BuildVariants(Request, Start + 10.0, Why)');
    expect(handler).toContain('In.Deadline = Start + 20.0;');
    expect(handler).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Assemble Modules")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());');
  });

  it('the dispatcher routes assemble_modules', () => {
    expect(strip(readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'McpEnvHandlers.cpp'), 'utf8')))
      .toMatch(/LowerSub == TEXT\("assemble_modules"\)\)\s*\{\s*return McpModuleAssembly::HandleAssembleModules\(\*this, RequestId, Payload, RequestingSocket\);/u);
  });
});
