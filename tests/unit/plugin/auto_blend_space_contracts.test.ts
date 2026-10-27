// Wiring contracts of animation_physics auto_blend_space that no unit test can reach by running it (it reads clips in
// the editor). The layout's known answers run in the editor (EngineWire.Animation.BlendLayout); behaviour belongs to the
// integration cases in tests/mcp-tools/core/auto-blend-space.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'AnimationAuthoring');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('auto_blend_space', () => {
  const handler = (): string => read(join(AUTHORING, 'Analysis', 'McpAnimAutoBlendSpace.cpp'));

  it('is routed to the animation authoring dispatcher', () => {
    expect(read(join(PRIVATE, 'MCP', 'Routing', 'McpConsolidatedActionRoutingAnimationSystem.h'))).toContain('TEXT("auto_blend_space")');
    expect(read(join(AUTHORING, 'McpAnimInfo.cpp'))).toMatch(/SubAction == TEXT\("auto_blend_space"\)\)\s*\{\s*return HandleAutoBlendSpace\(Params, Response\);/u);
  });

  it('checks replacing clips against each other, not against the samples about to go', () => {
    // ValidateAnimationSequence also matches a clip to the existing samples: replacing them, that refused a valid layout.
    expect(handler()).toMatch(/bKeep \? BlendSpace->ValidateAnimationSequence\(Clip\) : Clips\.Num\(\) == 0 \|\| Clips\[0\]->GetAdditiveAnimType\(\) == Clip->GetAdditiveAnimType\(\)/u);
  });

  it('sets the axes before adding samples and deletes samples from the last', () => {
    const source = handler();
    // AddSample widens an axis to hold a sample outside it, so the axes go first.
    expect(source.indexOf('bWrapInput = Axes[Axis].bWrap')).toBeGreaterThan(-1);
    expect(source.indexOf('bWrapInput = Axes[Axis].bWrap')).toBeLessThan(source.indexOf('AddSample(Clips[Clip], Position)'));
    expect(source).toMatch(/for \(int32 Index = bKeep \? -1 : BlendSpace->GetNumberOfBlendSamples\(\) - 1; Index >= 0; --Index\)\s*\{\s*BlendSpace->DeleteSample\(Index\);/u);
  });

  it('saves only after the transaction closes', () => {
    const source = handler();
    expect(source.indexOf('Transaction.DescribeInto(Response);')).toBeLessThan(source.indexOf('SaveAnimAssetAndReport(BlendSpace'));
  });
});
