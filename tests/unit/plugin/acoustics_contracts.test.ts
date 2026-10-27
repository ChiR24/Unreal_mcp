// Wiring contracts of manage_audio analyze_acoustics that no unit test can reach by running it (it traces a level). The
// room estimate's known answers run in the editor (EngineWire.Audio.Acoustics); behaviour belongs to the integration
// cases in tests/mcp-tools/core/acoustics.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const AUDIO = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Audio');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(AUDIO, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('analyze_acoustics', () => {
  const handler = (): string => read('McpAudioAcoustics.cpp');

  it('is routed by the runtime audio dispatcher', () => {
    const dispatcher = read('McpAudioHandlers.cpp');
    expect(dispatcher).toContain('Lower != TEXT("analyze_acoustics")');
    expect(dispatcher).toContain('&McpAudioHandlers::HandleAcousticsActions');
    expect(read('McpAudioPrivate.h')).toContain('bool HandleAcousticsActions(');
  });

  it('asks whether the listener is inside a solid of simple collision, which knows inside from outside', () => {
    // Complex collision is surfaces: a point inside a closed mesh overlaps none of its triangles, and the rays then
    // leave through its back faces as if the room had openings.
    expect(handler()).toMatch(/FCollisionQueryParams Solid\(SCENE_QUERY_STAT\(McpAcousticsSolid\), false, Listener\)/u);
    expect(handler()).toMatch(/OverlapBlockingTestByChannel\(Point, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere\(1\.0f\), Solid\)/u);
  });

  it('traces complex collision on the visibility channel past the listener actor, normals facing the ray', () => {
    const source = handler();
    expect(source).toMatch(/FCollisionQueryParams Params\(SCENE_QUERY_STAT\(McpAcoustics\), true, Listener\)/u);
    expect(source).toMatch(/LineTraceSingleByChannel\(Hit, From, From \+ Direction \* MaxDistance, ECC_Visibility, Params\) \|\| Hit\.bStartPenetrating/u);
    expect(source).toMatch(/DotProduct\(Hit\.ImpactNormal, Direction\) > 0\.0 \? -Hit\.ImpactNormal : Hit\.ImpactNormal/u);
  });

  it('includes FHitResult from its 5.0 home or its own header after', () => {
    expect(handler()).toMatch(/#if ENGINE_MAJOR_VERSION > 5 \|\| \(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1\)\s+#include "Engine\/HitResult\.h"\s+#else\s+#include "Engine\/EngineTypes\.h"\s+#endif/u);
  });
});
