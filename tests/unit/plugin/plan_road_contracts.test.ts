// Wiring contracts of build_environment plan_road that no unit test can reach by running it (it needs an editor and
// ground to trace). The search itself is pinned by the in-editor known answers (EngineWire.Environment.RoadPlan) and the
// integration cases in tests/mcp-tools/world/plan-road.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const ROADS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'Roads');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (source: string): string => source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (file: string): string => strip(readFileSync(join(ROADS, file), 'utf8'));

describe('plan_road', () => {
  const plan = read('McpRoadPlan.cpp');
  const smooth = read('McpRoadSmooth.cpp');
  const handler = read('McpEnvRoad.cpp');

  // One heading notch between one-cell edges turns at 2.12 cells: finer cells than 0.48 x the radius could not turn, and
  // a cell 3% larger could no longer climb a hill at the limit. Ends snapped to cell centres began and ended the road up to
  // half a cell diagonal off, so the grid is turned and sized to put both ends on its points.
  it('sizes the cell from the turn radius and turns the grid so both ends sit on its points', () => {
    expect(handler).toContain('const double Wanted = Distance / FMath::Max3(0.48 * Radius, (Distance + 2.0 * Margin) / 400.0, 100.0);');
    expect(handler).toContain('const int32 J = FMath::Min(I, FMath::FloorToInt(FMath::Sqrt(FMath::Max(0.0, Wanted * Wanted - I * I))));');
    expect(handler).toContain('Field.Cell = Distance / FMath::Sqrt(static_cast<double>(Reach.X * Reach.X + Reach.Y * Reach.Y));');
    expect(handler).toContain('const FIntPoint FromCell(Rim, Rim), ToCell(Rim + Reach.X, Rim + Reach.Y);');
    expect(handler).toContain('return From + Along * ((Cell.X - Rim) * Field.Cell) + Across * ((Cell.Y - Rim) * Field.Cell);');
  });

  it('allows a turn only where its corner keeps the radius, and an edge only under the grade along its length', () => {
    expect(plan).toContain('if (CornerRadius(RoadStepLength(In) * Field.Cell, RoadStepLength(Out) * Field.Cell, Turn) < RadiusCm)');
    expect(plan).toContain('return Turn < 1e-9 ? TNumericLimits<double>::Max() : FMath::Min(LengthA, LengthB) / (2.0 * FMath::Tan(Turn / 2.0));');
    expect(plan).toMatch(/if \(Steep > MaxGrade\)\s*\{\s*continue;/u);
    expect(plan).toContain('const int32 Samples = FMath::Max(1, FMath::CeilToInt(Length * 4.0));');
  });

  // A merge that only checked its own corner stranded the road where the lattice edge after it could not turn.
  it('straightens only where both new corners keep the radius', () => {
    expect(smooth).toContain('!RoadCornerHolds(Field, Kept[Kept.Num() - 2], Path[From], Path[To], RadiusCm)');
    expect(smooth).toContain('!RoadCornerHolds(Field, Path[From], Path[To], Path[To + 1], RadiusCm)');
  });

  // A long winding road made unbounded smoothing quadratic; a NO_ROUTE over open ground grew the frontier without end.
  it('bounds the work: a 64-step smoothing window, an early stop on grade, a frontier cap, both searches inside 22 s', () => {
    expect(smooth).toContain('for (int32 To = FMath::Min(Path.Num() - 1, From + 64); To > From + 1; --To)');
    expect(plan).toMatch(/if \(Run\.Grade > MaxGrade\)\s*\{\s*Run\.bClear = true;\s*return Run;/u);
    expect(plan).toContain('if (Frontier.Num() > RoadMaxFrontier ||');
    expect(handler).toContain('MaxGrade, Radius, Start + 16.0, Searched, bStopped);');
    expect(handler).toContain('10.0, Radius, Start + 22.0, Again, bAgainStopped);');
  });

  // One trace at each cell's middle let a 4 m wall between two cell centres through; a cell box at the ends would refuse
  // a road ending at a door.
  it('finds anything standing anywhere in a cell, and judges the two ends by their own points', () => {
    expect(handler).toContain('World->OverlapMultiByObjectType(Overlaps, FVector(At, OutZ + 180.0), Facing, Solids, FCollisionShape::MakeBox(FVector(HalfCell, HalfCell, 150.0)), Params);');
    expect(handler).toContain('RoadProbe(World, WorldAt(FIntPoint(X, Y)), Ground, Field.Heights[Y * Field.Width + X], Hit, Field.Cell / 2.0, Facing);');
    expect(handler).toContain('const ERoadGround FromKind = RoadProbe(World, From, Ground, FromZ, FromHit);');
    expect(handler).toContain('Field.Open[Y * Field.Width + X] = Kind == ERoadGround::Ground || bEnd ? 1 : 0;');
  });

  // In a World Partition level the traces hit streaming proxies, not the landscape actor a caller names.
  it('counts a named landscape through its streaming proxies', () => {
    expect(handler).toContain('Ground.Contains(Proxy->GetLandscapeActor())');
  });

  it('reads ground as the placement checks judge support, and keeps obstacles half the width away except round the ends', () => {
    expect(handler).toContain('return Actor && Part && !Actor->IsA<AVolume>() && Part->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block;');
    expect(handler).toContain('McpRoadPlan::Grow(Field, FMath::CeilToInt(Width / 2.0 / Field.Cell), {FromCell, ToCell});');
  });

  it('hands the route to the road spline and the landscape spline as ready calls', () => {
    expect(handler).toContain('RoadCall(TEXT("create_road_spline"), Road)');
    expect(handler).toContain('RoadCall(TEXT("configure_landscape_splines"), Shaped)');
    expect(handler).toContain('Spline->SetObjectField(TEXT("position"), At);');
  });

  it('the dispatcher routes plan_road', () => {
    expect(strip(readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Environment', 'McpEnvHandlers.cpp'), 'utf8')))
      .toMatch(/LowerSub == TEXT\("plan_road"\)\)\s*\{\s*return McpRoadPlanning::HandlePlanRoad\(\*this, RequestId, Payload, RequestingSocket\);/u);
  });
});
