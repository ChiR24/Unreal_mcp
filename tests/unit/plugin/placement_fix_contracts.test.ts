// control_actor fix_placement moves what audit_placement reports, by the audit's own check. Wiring contracts only: the
// behaviour is covered by the control-actor integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const ACTOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlActor');
const FIX = join(ACTOR, 'Placement', 'Fix');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('fix_placement', () => {
  const handler = code(FIX, 'McpPlacementFix.cpp');
  const round = code(FIX, 'McpPlacementFixRound.cpp');
  const solve = code(FIX, 'McpPlacementFixSolve.cpp');

  it('is routed by control_actor with the subsystem\'s own actor lookup', () => {
    expect(code(ACTOR, 'McpActorDispatch.cpp')).toMatch(
      /LowerSub == TEXT\("fix_placement"\)\)\s*return McpPlacementFix::HandleFixPlacement\(this, RequestId, Payload, RequestingSocket,\s*\[this\]\(const FString &Name\) \{ return FindActorByName\(Name\); \}\);/u,
    );
  });

  it('judges each actor with the audit\'s own placement check', () => {
    expect(round).toContain('McpPlacement::DescribePlacement(Actor, Entry);');
    expect(round).toContain('Finding.Lift = -Clearance;');
  });

  it('never moves a floor, a locked actor or an attached one, and refuses during Play', () => {
    expect(handler).toMatch(/McpPlacement::McpIsGroundLike\(Actor\)[\s\S]*Actor->IsLockLocation\(\)[\s\S]*Actor->GetAttachParentActor\(\)/u);
    expect(handler).toContain('if (!World || GEditor->PlayWorld)');
  });

  it('pushes only pairs it measured on their collision, with offsets planned before any actor moves', () => {
    expect(solve).toMatch(/if \(Total <= 0\.0 \|\| !McpPlacement::McpMeasurePush\(A\.Actor, B\.Actor, A\.Offset - B\.Offset, Push, Result\.Calls\)\)\s*\{\s*continue;/u);
    expect(solve).not.toMatch(/SetActorLocation|GetActorBounds/u);
    expect(round.indexOf('Separate(Bodies, Pairs,')).toBeLessThan(round.indexOf('SetActorLocation('));
  });

  it('makes every move in one undo step, bounded in queries and in time', () => {
    expect(handler).toContain('const FScopedTransaction Transaction(NSLOCTEXT("McpPlacementFix", "FixPlacement", "Fix Placement"), !bDryRun);');
    expect(handler.indexOf('const FScopedTransaction Transaction(')).toBeLessThan(handler.indexOf('Run.Round('));
    expect(round).toContain('constexpr int32 MaxPenetrationCalls = 4096;');
    expect(handler).toContain('constexpr double RoundDeadlineSeconds = 15.0;');
  });
});
