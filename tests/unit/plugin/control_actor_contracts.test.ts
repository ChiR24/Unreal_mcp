// Wiring contracts of the control_actor handlers that no unit test can reach by running
// them (they need an editor). Behaviour itself belongs to the integration cases in
// tests/mcp-tools/core/control-actor.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const DOMAIN = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'ControlActor');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function read(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(DOMAIN, ...segments), 'utf8'));
}

function listParamDescription(param: string): string {
  const properties = capabilityIndex().byId.get('control_actor.list')?.schemas.input.properties;
  const entry = isRecord(properties) ? properties[param] : undefined;
  return isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';
}

describe('control_actor.list near and radius: what is close to a point, nearest first', () => {
  const list = (): string => read('List', 'McpAutomationBridge_ControlActorList.cpp');
  const record = () => capabilityIndex().byId.get('control_actor.list');

  it('lives in its own file, not in the lookup file it was moved from', () => {
    expect(list()).toContain('bool UMcpAutomationBridgeSubsystem::HandleControlActorList(');
    expect(read('McpAutomationBridge_ControlActorLookup.cpp')).not.toContain('HandleControlActorList');
  });

  it('measures the distance to the actor\'s world bounding box: 0 inside it, per axis outside it', () => {
    const source = list();

    expect(source).toMatch(/Actor->GetActorBounds\(false, Origin, Extent\);/u);
    expect(source).toMatch(/const FVector Outside = \(Point - Origin\)\.GetAbs\(\) - Extent;/u);
    expect(source).toMatch(/return FVector\(FMath::Max\(Outside\.X, 0\.0\), FMath::Max\(Outside\.Y, 0\.0\), FMath::Max\(Outside\.Z, 0\.0\)\)\.Size\(\);/u);
  });

  it('filters and applies the radius first, sorts by distance (the smaller box, then path, breaks ties), then pages', () => {
    const source = list();
    const collect = source.indexOf('Listed.Add(');
    const sort = source.indexOf('Listed.Sort(');
    const page = source.indexOf('for (const FMcpListedActor &Item : Listed)');

    expect(collect).toBeGreaterThan(-1);
    expect(sort).toBeGreaterThan(collect);
    expect(page).toBeGreaterThan(sort);
    expect(source).toMatch(/McpActorMatchesListFilters\(Actor, TagFilter, ClassFilter, FolderFilter\)/u);
    expect(source).toMatch(/if \(bRadius && Distance > Radius\)\s*continue;/u);
    // Equally near actors (all containing the point, say) put the smaller box first, so the thing at the
    // spot leads and the level-wide foliage actor, which contains every point, comes last.
    expect(source).toMatch(/if \(A\.Distance != B\.Distance\) return A\.Distance < B\.Distance;\s*if \(A\.BoundsSize != B\.BoundsSize\) return A\.BoundsSize < B\.BoundsSize;\s*return A\.Actor->GetPathName\(\)\.Compare\(B\.Actor->GetPathName\(\)\) < 0;/u);
    expect(source).toMatch(/OutBoundsSize = Extent\.Size\(\);/u);
  });

  // className /Game/Enemies/BP_Bug listed 0 actors: ObjectPathToObjectName hands a path without a
  // '.' back whole, and no class is named after a whole path.
  it('className matches a Blueprint by package path, as well as by name or object path', () => {
    expect(read('McpAutomationBridge_ControlActorSupport.h'))
      .toContain('FString Wanted = FPackageName::GetShortName(FPackageName::ObjectPathToObjectName(ClassName));');
  });

  it('a row carries its distance only when near was given', () => {
    expect(list()).toMatch(/if \(bNear\)\s*Entry->SetNumberField\(TEXT\("distance"\), FMath::RoundToDouble\(Item\.Distance \* 10\.0\) \/ 10\.0\);/u);
  });

  it('refuses a near that is not a point, a negative radius, and a radius with no near', () => {
    const source = list();

    expect(source).toMatch(/!ReadJsonTriple\(Payload->TryGetField\(TEXT\("near"\)\), Axes, Point\)/u);
    expect(source).toMatch(/if \(bRadius && \(!bNear \|\| Radius < 0\.0\)\)/u);
    expect((source.match(/TEXT\("INVALID_ARGUMENT"\)/gu) ?? []).length).toBeGreaterThanOrEqual(2);
  });

  it('the record declares near, radius and the row distance, and says it finds what is near a point', () => {
    const properties = record()?.schemas.input.properties;
    const declared = isRecord(properties) ? Object.keys(properties) : [];
    const rows = record()?.schemas.output.properties?.actors;
    const rowProperties = isRecord(rows) && isRecord(rows.items) && isRecord(rows.items.properties) ? Object.keys(rows.items.properties) : [];

    expect(declared).toEqual(expect.arrayContaining(['near', 'radius']));
    expect(rowProperties).toContain('distance');
    expect(record()?.discovery.summary).toContain('find what is near a point');
    expect(listParamDescription('radius')).toMatch(/comes within this distance of the point/u);
  });
});

describe('control_actor.list propertyNames takes "Component.Property" like sample_motion', () => {
  it('both handlers resolve a name through the one shared resolver', () => {
    const support = read('McpAutomationBridge_ControlActorSupport.h');
    const list = read('List', 'McpAutomationBridge_ControlActorList.cpp');
    const motion = read('McpAutomationBridge_ControlActorMotionSample.cpp');

    expect(support).toMatch(/inline FProperty \*McpResolveActorPropertyPath\(AActor \*Actor, const FString &Wanted, UObject \*&OutOwner\)/u);
    expect(support).toMatch(/Wanted\.Split\(TEXT\("\."\), &ComponentName, &PropertyName\)\)\s*\{\s*OutOwner = FindComponentByName\(Actor, ComponentName\);/u);
    expect(list).toMatch(/McpResolveActorPropertyPath\(Actor, Wanted, Owner\)/u);
    expect(motion).toMatch(/McpResolveActorPropertyPath\(Found, Wanted, Owner\)/u);
    expect(motion, 'sample_motion no longer carries its own copy of the resolution').not.toMatch(/Wanted\.Split\(/u);
  });

  it('list reads the value off the resolved owner and keys a component property as asked', () => {
    const list = read('List', 'McpAutomationBridge_ControlActorList.cpp');

    expect(list).toMatch(/GetPropertyValueAsString\(Owner, Property\)/u);
    expect(list).toMatch(/Wanted\.Contains\(TEXT\("\."\)\) \? Wanted : Property->GetName\(\)/u);
    expect(list).toMatch(/Missing\.Add\(MakeShared<FJsonValueString>\(Wanted\)\)/u);
  });

  it('the propertyNames description says so', () => {
    expect(listParamDescription('propertyNames')).toMatch(/"Component\.Property"/u);
    expect(listParamDescription('propertyNames')).toMatch(/StaticMeshComponent\.LDMaxDrawDistance/u);
  });
});

// startWhen {equals: 2} (or "2") waited for "True": TryGetBoolField coerces numbers and strings.
describe('startWhen.equals is read by its JSON type', () => {
  const source = read('McpAutomationBridge_ControlActorMotionInputs.cpp');

  it('dispatches on the value type instead of trying a bool first', () => {
    expect(source).toContain('When->TryGetField(TEXT("equals"))');
    expect(source).toMatch(/EqualsType == EJson::Number\) \{\s*Out\.Equals = FString::SanitizeFloat\(EqualsValue->AsNumber\(\)\);/u);
    expect(source).not.toMatch(/TryGetBoolField\(TEXT\("equals"\)/u);
  });
});

// A minimized editor stepped PIE 0.333 s at a time: a 0.22 s jump was held 0.67 s and the
// samples read as the level's own behaviour, with nothing in the reply to say so.
describe('sample_motion runs at full rate or says it did not', () => {
  const source = read('McpAutomationBridge_ControlActorMotionSample.cpp');

  it('puts a minimized editor back on screen without focus before the run', () => {
    expect(source).toContain('Run->bWindowRestored = RestoreWindowForCaptureForMcp(Root.ToSharedRef());');
    expect(source).toContain('Data->SetBoolField(TEXT("windowRestored"), true);');
  });

  it('warns when the game advanced 0.1 s or more per frame', () => {
    expect(source).toContain('Run.Frames += 1;');
    expect(source).toMatch(/if \(PerFrame < 0\.1\) \{\s*return FString\(\);/u);
    expect(source).toContain('McpSlowFrameWarning(*Run)');
  });
});

// "Bug1" found nothing among Bug_01..Bug_10 and the empty list read as "there are no bugs".
describe('find_by_name offers similar labels when nothing matches', () => {
  it('compares names without case, separators and leading zeros, only after a miss', () => {
    const source = read('McpAutomationBridge_ControlActorQuery.cpp');
    expect(source).toContain('if (Matches.Num() == 0) {');
    expect(source).toContain('FChar::IsAlnum(In[I]) && !bLeadingZero');
    expect(source).toContain('Data->SetArrayField(TEXT("similar"), SimilarValues);');
  });
});
