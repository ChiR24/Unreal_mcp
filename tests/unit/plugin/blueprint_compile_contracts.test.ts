// A Blueprint compile re-instances the editor world's placed copies of it, and their skeletal meshes came back with
// every bone at identity: each part of a character drawn at the actor's origin, and every capture of the level wrong,
// until the world ticked, which an editor kept minimized never does. Every compile the plugin runs goes through
// McpCompileBlueprint, which poses those meshes again. Needs an editor to run, so these assert the wiring.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const SOURCE = join('plugins', 'EngineWire', 'Source');
const HELPERS = join(SOURCE, 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const helpers = (): string => code(join(HELPERS, 'Blueprints', 'McpBPCompileHelpers.h'));
const compile = (): string =>
  sliceBetween(helpers(), 'static inline void McpCompileBlueprint(', 'static inline bool McpSafeCompileBlueprint(');

describe('every Blueprint compile goes through McpCompileBlueprint', () => {
  it('reaches the engine compile from that one place, apart from Blueprints nothing placed is an instance of', () => {
    const callers = readdirSync(SOURCE, { recursive: true, encoding: 'utf8' })
      .filter((file) => /\.(?:cpp|h)$/u.test(file))
      .filter((file) => /FKismetEditorUtilities::CompileBlueprint\(|FBlueprintCompilationManager::/u.test(code(join(SOURCE, file))))
      .map((file) => file.replace(/\\/gu, '/'))
      .sort();
    expect(callers).toEqual([
      // A Control Rig's class is not an actor class.
      'EngineWire/Private/Domains/AnimationAuthoring/ControlRig/McpControlRigGraphEdit.cpp',
      // The copy the same call made: nothing of it is placed yet.
      'EngineWire/Private/Domains/BlueprintGraph/Expression/McpBPGraphMathExprCopy.cpp',
      'EngineWire/Private/Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h',
    ]);
  });

  it('the compile helpers the handlers share compile through it', () => {
    expect(sliceBetween(helpers(), 'static inline bool McpSafeCompileBlueprint(', 'return Blueprint->Status'))
      .toContain('McpCompileBlueprint(Blueprint, OutError ? &Results : nullptr);');
    expect(sliceBetween(code(join(HELPERS, 'Blueprints', 'McpBPDiag.h')), 'inline bool McpCompileBlueprintWithDiagnostics(', 'const bool bCompiled'))
      .toContain('McpCompileBlueprint(Blueprint, &Results);');
    expect(code(join(HELPERS, 'Blueprints', 'McpBPCompileError.h'))).toContain('return McpSafeCompileBlueprint(Blueprint, &OutError);');
  });
});

describe('a compile poses the skeletal meshes of the Blueprint\'s placed copies again', () => {
  it('after the compile, on each copy in the editor world, a child Blueprint\'s included', () => {
    const body = compile();
    const engine = body.indexOf('FKismetEditorUtilities::CompileBlueprint(Blueprint, Options, Results);');
    expect(engine).toBeGreaterThan(-1);
    expect(body.indexOf('UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;')).toBeGreaterThan(engine);
    expect(body).toContain('if (!Class || !World || !Class->IsChildOf(AActor::StaticClass())) {');
    // TActorIterator takes the class's subclasses too: the same compile re-instances a child Blueprint's copies.
    expect(body).toContain('for (TActorIterator<AActor> It(World, Class); It; ++It) {');
    expect(body).toContain('TInlineComponentArray<USkeletalMeshComponent *> Meshes(*It);');
    expect(body).toContain('McpRefreshComponentAfterEdit(Mesh);');
  });

  it('twice for a copy with several meshes, so one that copies another\'s pose reads it posed', () => {
    expect(compile()).toMatch(/const int32 Passes = Meshes\.Num\(\) > 1 \? 2 : 1;\s*for \(int32 Pass = 0; Pass < Passes; \+\+Pass\)/u);
  });

  it('dirties no package and records no undo step', () => {
    const lookup = code(join(HELPERS, 'Properties', 'McpComponentLookup.h'));
    const refresh = sliceBetween(lookup, 'static inline void McpRefreshComponentAfterEdit(', 'static inline bool McpIsCollisionProfile(');
    expect(refresh).toContain('Skeletal->InitAnim(true);');
    for (const body of [compile(), refresh]) {
      expect(body).not.toMatch(/Modify\(|MarkPackageDirty|SetDirtyFlag|Transaction/u);
    }
  });
});
