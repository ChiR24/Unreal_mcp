// control_actor describe_relations reads the level back as relations between its actors. Wiring and structure contracts
// only: the behaviour is covered by the control-actor integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const ACTOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlActor');
const RELATIONS = join(ACTOR, 'Relations');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('describe_relations', () => {
  const handler = code(RELATIONS, 'McpRelations.cpp');
  const pairs = code(RELATIONS, 'McpRelationsPairs.cpp');

  it('is routed by control_actor with the subsystem\'s own actor lookup', () => {
    expect(code(ACTOR, 'McpActorDispatch.cpp')).toMatch(
      /LowerSub == TEXT\("describe_relations"\)\)\s*return McpRelations::HandleDescribeRelations\(this, RequestId, Payload, RequestingSocket,\s*\[this\]\(const FString &Name\) \{ return FindActorByName\(Name\); \}\);/u,
    );
  });

  it('changes nothing: no transaction, no move, no edit', () => {
    for (const file of ['McpRelations.cpp', 'McpRelationsPairs.cpp', 'McpRelationsPatterns.cpp', 'McpRelationsCopies.cpp', 'McpRelationsRoles.cpp']) {
      expect(code(RELATIONS, file)).not.toMatch(/FScopedTransaction|SetActorLocation|SetActorRotation|->Modify\(|MarkPackageDirty/u);
    }
  });

  // An editor-only sprite made an 84 cm prop a 256 cm cube; the same box describe_view reads.
  it('judges an actor by what the game draws of it, and keeps the placement checks\' bounds-only actors out', () => {
    expect(handler).toContain('McpPlacement::IsBoundsOnlyActor(Actor) ? FBox(ForceInit) : McpPlacement::DrawnBounds(*Actor)');
    expect(code('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'Inspection', 'McpEnvInspectViewUnseen.cpp'))
      .toContain('McpPlacement::DrawnBounds(*Actor)');
  });

  it('finds candidates through a grid over the ground plane, never every pair', () => {
    expect(pairs).toContain('for (const int32 B : Grid.Near(Item.Box.ExpandBy(FVector(Reach, Reach, 0.0))))');
    expect(pairs).toContain('constexpr int32 MaxCellsPerItem = 256;');
  });

  it('caps the actors judged and the relations listed', () => {
    expect(handler).toContain('FMath::Clamp(static_cast<int32>(MaxActors), 1, 400)');
    expect(handler).toContain('constexpr int32 MaxRelations = 200;');
  });

  // A floor under every prop drowned the rows that say something; what rests on the ground is listed when asked.
  it('lists what rests on the ground only for the actors asked about', () => {
    expect(handler).toContain('(Items[Relation.Object].bGround && !(bFocused && IsAsked[Relation.Subject]))');
  });

  // Every prop has an X axis: across a level, faces named a "front" for most of them and filled the 200 rows.
  it('lists faces when kinds names it or the report is narrowed to named actors', () => {
    expect(handler).toMatch(/if \(Kinds\.Num\(\) == 0\)\s*\{\s*for \(const TCHAR\* Kind : AllKinds\)\s*\{\s*if \(bFocused \|\| FCString::Strcmp\(Kind, TEXT\("faces"\)\) != 0\)/u);
  });

  it('never calls a pair that rests or hangs one on the other nextTo or facing, nor copies facing each other', () => {
    expect(pairs).toContain('return Support[A] == B || Support[B] == A || Mount[A] == B || Mount[B] == A;');
    expect(pairs).toMatch(/!Joined\(A, B\) &&\s*Other\.Shape != Item\.Shape\)/u);
  });
});

// control_actor arrange_by_relations moves actors until stated relations hold. Wiring contracts only: the solver's known
// answers are the in-editor Anneal test, the arrangements the integration cases.
describe('arrange_by_relations', () => {
  const handler = code(RELATIONS, 'McpArrange.cpp');
  const solve = code(RELATIONS, 'McpArrangeSolve.cpp');

  // A fuzzy name ("Chair" finding "Chair_Copy") would move the wrong actor.
  it('is routed by control_actor with an exact-name lookup', () => {
    expect(code(ACTOR, 'McpActorDispatch.cpp')).toMatch(
      /LowerSub == TEXT\("arrange_by_relations"\)\)\s*return McpRelations::HandleArrangeByRelations\(this, RequestId, Payload, RequestingSocket,\s*\[this\]\(const FString &Name\) \{ return FindActorByName\(Name, true\); \}\);/u,
    );
  });

  it('reads relations in execute\'s assertion language, against included', () => {
    expect(handler).toContain('McpPratt::Parse(Texts[Index], Root, Why)');
    expect(code('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Solvers', 'McpPratt.cpp')).toMatch(/Relations\[\] = \{[^}]*TEXT\("against"\)/u);
    expect(code(RELATIONS, 'McpRelationsJudge.cpp')).toContain('if (Kind == TEXT("nextTo") || Kind == TEXT("against"))');
  });

  it('moves in one undo step, and dryRun moves nothing', () => {
    expect(handler).toContain('const FScopedTransaction Transaction(NSLOCTEXT("McpRelations", "ArrangeByRelations", "Arrange By Relations"), !bDryRun);');
    expect(handler).toMatch(/if \(!bDryRun\)\s*\{\s*FRotator Rotation = Body\.Actor->GetActorRotation\(\);[\s\S]*?Body\.Actor->Modify\(\);\s*Body\.Actor->SetActorLocationAndRotation\(/u);
    expect(handler.match(/SetActorLocationAndRotation\(/gu)).toHaveLength(1);
  });

  it('never moves a floor, a locked or an attached actor, by fix_placement\'s own rule', () => {
    expect(handler).toContain('const FString Why = McpPlacementFix::PinnedReason(Actor);');
  });

  // The model can call a relation met that the engine would not: every verdict of a real run is the engine's.
  it('judges each relation after the move as expect judges it', () => {
    expect(handler).toContain('JudgePair(World, Asked[Index].Subject, Asked[Index].Kind, Asked[Index].Object, Asked[Index].Within, Actual, NextCall)');
  });

  it('answers well inside the client\'s 30 s limit', () => {
    expect(handler).toContain('constexpr double ArrangeSeconds = 8.0;');
    expect(solve).toContain('Settings.DeadlineSeconds = DeadlineSeconds;');
  });
});
