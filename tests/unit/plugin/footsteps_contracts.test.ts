// Wiring contracts of animation_physics add_footsteps that no unit test can reach by running it (it reads clips in the
// editor). Behaviour belongs to the integration cases in tests/mcp-tools/core/footsteps.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'AnimationAuthoring');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('add_footsteps', () => {
  const handler = (): string => read(join(AUTHORING, 'Analysis', 'McpAnimFootsteps.cpp'));

  it('is routed to the animation authoring dispatcher', () => {
    expect(read(join(PRIVATE, 'MCP', 'Routing', 'McpConsolidatedActionRoutingAnimationSystem.h'))).toContain('TEXT("add_footsteps")');
    expect(read(join(AUTHORING, 'McpAnimSequenceEvents.cpp'))).toMatch(/SubAction == TEXT\("add_footsteps"\)\)\s*\{\s*return HandleAddFootsteps\(Params, Response\);/u);
  });

  it('replaces only the Play Sound notifies an earlier run named Footstep', () => {
    const source = handler();
    expect(source).toMatch(/IsFootstep = \[\]\(const FAnimNotifyEvent& Event\)\s*\{\s*return Event\.NotifyName == FootstepNotifyName && Cast<UAnimNotify_PlaySound>\(Event\.Notify\) != nullptr;/u);
    expect(source).toContain('Sequence->Notifies.RemoveAll(IsFootstep);');
  });

  it('changes nothing for a clip with no step and no earlier footstep, and saves only after the transaction closes', () => {
    const source = handler();
    // An idle answered saved true and opened a transaction that recorded nothing.
    expect(source).toMatch(/if \(bChanges\)\s*\{\s*const FMcpScopedEditorTransaction Transaction/u);
    expect(source).toContain('SaveAnimAssetAndReport(Sequence, bChanges && GetJsonBoolField(Params, TEXT("save"), true), Response);');
    expect(source.indexOf('Transaction.DescribeInto(Response);')).toBeGreaterThan(-1);
    expect(source.indexOf('Transaction.DescribeInto(Response);')).toBeLessThan(source.indexOf('SaveAnimAssetAndReport(Sequence'));
  });
});
