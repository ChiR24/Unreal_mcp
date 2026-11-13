/**
 * Source contracts for the actor finders a path-restricted principal goes through.
 *
 * Foundation's finders (FindActorByNameInWorldForMcp, FindActorOfClassForMcp) leave out an actor whose level lies
 * outside the caller's allowed prefixes. A domain that matched a label in a loop of its own, or found an actor by
 * FindObject anywhere in memory, let such a caller change actors of any level. Each site below either calls a shared
 * finder or asks McpCapabilityPrincipal::ActiveRequestPermitsObject itself, and its private loop is gone.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const privateRoot = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private');
const code = (file: string): string =>
  readFileSync(resolve(privateRoot, file), 'utf8')
    .replace(/\/\*[\s\S]*?\*\//gu, '')
    .replace(/^[ \t]*\/\/.*$/gmu, '');
const RULE = 'McpCapabilityPrincipal::ActiveRequestPermitsObject';

describe('actor finders keep a path-restricted caller inside its prefixes', () => {
  it('Geometry finds its actors through the shared class finder', () => {
    expect(code('Domains/Geometry/McpGeometryHandlers.h')).not.toContain('FindGeometryActor');
    for (const file of [
      'Domains/Geometry/Modeling/McpGeometryBooleans.cpp',
      'Domains/Geometry/Modeling/McpGeometryCurveBridge.cpp',
      'Domains/Geometry/Support/McpGeometryActorSupport.cpp',
      'Domains/Geometry/Support/McpGeometryUndo.cpp',
    ]) {
      expect(code(file), file).toContain('FindActorOfClassForMcp<');
    }
  });

  it('LOD settings ask the rule for the actor and for the mesh asset they save', () => {
    const lod = code('Domains/Geometry/Mesh/McpGeometryLodOperations.cpp');
    expect(lod).toContain(`${RULE}(*It)`);
    expect(lod).toContain(`${RULE}(Mesh)`);
    expect(lod).toContain('PATH_NOT_PERMITTED');
  });

  it('sites that walk the world for an unnamed or labelled target ask the rule', () => {
    for (const file of [
      'Domains/GameFramework/McpGameFrameworkPlayerFlow.cpp',
      'Domains/Landscape/McpLandscapeLookup.cpp',
      'Domains/Lighting/McpLightingSky.cpp',
      'Domains/PCG/McpPCGWorld.cpp',
      'Domains/Performance/Budget/McpFrameBudgetKnobs.cpp',
      'Domains/Render/McpRenderSupport.h',
      'Domains/Render/McpRenderReflectResolve.cpp',
      'Foundation/BridgeHelpers/Actors/McpActorSpawnHelpers.cpp',
      'Foundation/Render/McpPostProcessVolumeResolution.cpp',
    ]) {
      expect(code(file), file).toContain(RULE);
    }
  });

  it('the landscape lookups are one finder that never loads a level to look', () => {
    const lookup = code('Domains/Landscape/McpLandscapeLookup.cpp');
    expect(lookup).not.toContain('FindLandscapeForEdit');
    expect(lookup).not.toContain('StaticLoadObject');
  });

  it('no finder takes an actor from anywhere in memory by FindObject', () => {
    for (const file of [
      'Domains/Render/McpRenderSupport.h',
      'Domains/Performance/McpPerfActorMergeSupport.cpp',
      'Domains/Performance/McpPerfActorMerge.cpp',
    ]) {
      expect(code(file), file).not.toMatch(/FindObject<AActor>\(nullptr/u);
    }
  });

  it('describe and invoke of a reflected function pick their object through one principal-checked resolver', () => {
    const api = code('Domains/ControlEditor/McpEditorReflectedApi.cpp');
    expect(api).toContain('UObject* McpFindReflectedInstance(const FString& ClassName)');
    expect(api).toContain('!Candidate->IsTemplate()');
    expect(api).toContain(`${RULE}(Candidate)`);
    const invoke = code('Domains/ControlEditor/McpEditorReflectedInvoke.cpp');
    expect(invoke).toContain('McpFindReflectedInstance(ClassName)');
    expect(invoke).not.toContain('TObjectIterator');
  });

  it('nav links, nav areas, morph targets, volumes and merges go through the shared finders', () => {
    expect(code('Domains/Navigation/McpNavPrivate.h')).not.toContain('FindNavLinkProxyByName');
    expect(code('Domains/Navigation/McpNavLinks.cpp')).toContain('FindActorOfClassForMcp<ANavLinkProxy>');
    expect(code('Domains/Navigation/McpNavModifiers.cpp')).not.toContain('TActorIterator<AActor>');
    expect(code('Domains/Skeleton/MorphTargets/McpSkeletonMorphTargetQueries.cpp')).toContain('FindActorByNameInWorldForMcp(');
    const volumes = code('Domains/Volume/McpVolumeWorldResolve.cpp');
    expect(volumes).toContain('FindActorByNameInWorldForMcp(');
    expect(volumes).not.toContain('EndsWith(');
    expect(code('Domains/Performance/McpPerfActorMerge.cpp')).toContain('FindActorByNameInWorldForMcp(');
  });
});
