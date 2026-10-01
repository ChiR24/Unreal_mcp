// Wiring contracts of the control_actor handlers that no unit test can reach by running
// them (they need an editor). Behaviour itself belongs to the integration cases in
// tests/mcp-tools/core/control-actor.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { unreadVariantParams } from '../../../src/server/gateway/gateway-dispatch-by.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';
import { extractChanges, extractHandles } from '../../../src/tools/catalog/capabilities/semantic/receipt-outcome.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

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

// Hiding seventeen actors took thirty-four calls: set_visibility and set_actor_collision took one actorName
// each, while add_tag already took actorNames and listed the names it could not find.
describe('set_visibility and set_actor_collision take many actors in one call, as add_tag does', () => {
  const support = (): string => read('McpAutomationBridge_ControlActorSupport.h');
  const from = (file: string, marker: string): string => {
    const source = read(file);
    return source.slice(source.indexOf(marker));
  };
  const visibility = (): string => from('McpAutomationBridge_ControlActorTransform.cpp', 'HandleControlActorSetVisibility(');
  const collision = (): string => from('McpAutomationBridge_ControlActorPhysics.cpp', 'HandleControlActorSetCollision(');
  const addTag = (): string => sliceBetween(read('McpAutomationBridge_ControlActorTags.cpp'), 'HandleControlActorAddTag(', 'HandleControlActorRemoveTag(');

  it('one helper resolves the list, once per actor, and names what matched nothing; add_tag shares it', () => {
    const helper = sliceBetween(support(), 'inline bool McpResolveActorNames(', 'inline void McpSendNoActorNamesFound(');

    expect(helper).toContain('McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"))');
    expect(helper).toContain('OutActors.AddUnique(Actor);');
    expect(helper).toContain('OutMissing.Add(Name);');
    expect(helper).toContain('return Names.Num() > 0;');
    for (const [name, source] of [['add_tag', addTag()], ['set_visibility', visibility()], ['set_actor_collision', collision()]] as const) {
      expect(source, name).toMatch(/McpResolveActorNames\(\s*(?:Payload|Payload, \[this\])/u);
      expect(source, name).toMatch(/\[this\]\(const FString ?& ?Name\) \{ return FindActorByName\(Name\); \}/u);
    }
    expect(read('McpAutomationBridge_ControlActorTags.cpp'), 'add_tag no longer carries its own copy of the list').not.toContain('TryGetArrayField(TEXT("actorNames")');
  });

  it('names none found ACTOR_NOT_FOUND with the names listed back; some found is a success that lists the rest under missing', () => {
    const refusal = sliceBetween(support(), 'inline void McpSendNoActorNamesFound(', 'inline bool McpApplyActorVisibility(');

    expect(refusal).toContain('TEXT("ACTOR_NOT_FOUND")');
    expect(refusal).toContain('Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));');
    for (const source of [visibility(), collision()]) {
      expect(source).toMatch(/if \(bMany && Actors\.Num\(\) == 0\) \{\s*McpSendNoActorNamesFound\(this, Socket, RequestId, Missing\);\s*return true;/u);
      expect(source).toContain('Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));');
    }
  });

  it('set_visibility holds every named actor and its primitive components in ONE transaction, opened before the first write', () => {
    const source = visibility();
    const collect = source.indexOf('McpAddActorUndoSet(Actor, Undoable);');
    const open = source.indexOf('FMcpScopedEditorTransaction Transaction(');
    const write = source.indexOf('McpApplyActorVisibility(Actor, bVisible)');
    const undoSet = sliceBetween(support(), 'inline void McpAddActorUndoSet(', 'struct FMcpMotionInput');

    expect(source.match(/FMcpScopedEditorTransaction Transaction\(/gu)).toHaveLength(1);
    expect(source).toMatch(/for \(AActor \*Actor : Actors\) \{\s*McpAddActorUndoSet\(Actor, Undoable\);\s*\}/u);
    expect(collect).toBeGreaterThan(-1);
    expect(collect).toBeLessThan(open);
    expect(open).toBeLessThan(write);
    expect(undoSet).toContain('Add(Actor);');
    expect(undoSet).toContain('Add(Prim);');
    expect(source).toMatch(/Undoable\);\s*TArray<FString> Affected;\s*TArray<FString> Mismatched;/u);
  });

  // set_visibility on a Blueprint actor's BillboardComponent answered undoable:false: the transaction
  // gate refuses an object without RF_Transactional, and that component has none.
  it('flags each actor and primitive component that lacks RF_Transactional before the transaction opens, so its undo is recorded', () => {
    const undoSet = sliceBetween(support(), 'inline void McpAddActorUndoSet(', 'struct FMcpMotionInput');
    const source = visibility();

    expect(undoSet).toMatch(/if \(!Object->HasAnyFlags\(RF_Transactional\)\) \{\s*Object->SetFlags\(RF_Transactional\);\s*\}\s*Undoable\.Add\(Object\);/u);
    expect(undoSet).toMatch(/Add\(Actor\);\s*for \(UActorComponent \*Comp : Actor->GetComponents\(\)\) \{\s*if \(UPrimitiveComponent \*Prim = Cast<UPrimitiveComponent>\(Comp\)\) \{\s*Add\(Prim\);/u);
    expect(source.indexOf('McpAddActorUndoSet(Actor, Undoable);')).toBeLessThan(source.indexOf('FMcpScopedEditorTransaction Transaction('));
  });

  it('a mismatch on any actor fails the call naming it; the single-actor replies keep their shape', () => {
    const source = visibility();

    expect(source).toMatch(/if \(McpApplyActorVisibility\(Actor, bVisible\)\) \{\s*Affected\.Add\(McpActorRef\(Actor\)\);\s*\} else \{\s*Mismatched\.Add\(McpActorRef\(Actor\)\);/u);
    expect(source).toContain('TEXT("VISIBILITY_MISMATCH")');
    expect(source).toContain('Data->SetStringField(TEXT("actorName"), McpActorRef(Actors[0]));');
    expect(source).toContain('McpHandlerUtils::AddVerification(Data, Actors[0]);');
    expect(source).toContain('FString(TEXT("Actor visibility updated"))');
    expect(collision()).toContain('SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);');
  });

  it('set_actor_collision toggles every named actor and each of its primitive components, and lists the actors that have none', () => {
    const source = collision();

    expect(source).toMatch(/for \(AActor\* Actor : Actors\)\s*\{\s*Actor->SetActorEnableCollision\(bCollisionEnabled\);\s*TInlineComponentArray<UPrimitiveComponent\*> PrimComponents\(Actor\);/u);
    expect(source).toContain('Updated += OnActor;');
    expect(source).toContain('NoComponent.Add(McpActorRef(Actor));');
    expect(source).toContain('Data->SetArrayField(TEXT("noPrimitiveComponents")');
    expect(source).toContain('TEXT("NO_COMPONENT")');
  });

  it('both records declare actorNames and take one of actorName or actorNames', () => {
    for (const id of ['control_actor.set_visibility', 'control_actor.set_actor_collision']) {
      const input = capabilityIndex().byId.get(id)?.schemas.input;

      expect(isRecord(input?.properties) ? input.properties.actorNames : undefined, id).toMatchObject({ type: 'array', items: { type: 'string' } });
      expect(input?.requiredOneOf, id).toEqual(['actorName', 'actorNames']);
      expect(input?.required, id).not.toContain('actorName');
    }
  });
});

// A world actor's mesh and materials took an inspect_object call per actor to find;
// only a Blueprint's own components listed them.
describe('get_components rows name the mesh and materials a component draws', () => {
  it('world rows add the shared mesh asset fields', () => {
    expect(read('McpAutomationBridge_ControlActorComponentDetails.cpp')).toContain('McpHandlerUtils::AddMeshAssetFields(Component, Entry);');
  });
});

// set_actor_collision opened no transaction, one actor or many, so control_editor.undo had nothing to take
// back, while set_visibility already was one undo step.
describe('set_actor_collision is one undoable step over every actor it changes, as set_visibility is', () => {
  const support = (): string => read('McpAutomationBridge_ControlActorSupport.h');
  const from = (file: string, marker: string): string => {
    const source = read(file);
    return source.slice(source.indexOf(marker));
  };
  const collision = (): string => from('McpAutomationBridge_ControlActorPhysics.cpp', 'HandleControlActorSetCollision(');
  const visibility = (): string => from('McpAutomationBridge_ControlActorTransform.cpp', 'HandleControlActorSetVisibility(');

  it('builds the shared undo set for each actor and opens ONE named transaction, before the first write', () => {
    const source = collision();
    const collect = source.indexOf('McpAddActorUndoSet(Actor, Undoable);');
    const open = source.indexOf('FMcpScopedEditorTransaction Transaction(');
    const write = source.indexOf('Actor->SetActorEnableCollision(bCollisionEnabled);');

    expect(source.match(/FMcpScopedEditorTransaction Transaction\(/gu)).toHaveLength(1);
    expect(source).toMatch(/for \(AActor\* Actor : Actors\)\s*\{\s*McpAddActorUndoSet\(Actor, Undoable\);\s*\}/u);
    expect(source).toMatch(/FMcpScopedEditorTransaction Transaction\(\s*FText::FromString\(TEXT\("Set Actor Collision"\)\),\s*EMcpMutationDurability::EditorStateOnly, Undoable\);/u);
    expect(collect).toBeGreaterThan(-1);
    expect(collect).toBeLessThan(open);
    expect(open).toBeLessThan(write);
    expect(read('McpAutomationBridge_ControlActorPhysics.cpp')).toContain('#include "Foundation/McpScopedEditorTransaction.h"');
  });

  it('the undo block rides in the reply of the single and of the many form alike', () => {
    const source = collision();

    expect(source.match(/Transaction\.DescribeInto\(Data\);/gu)).toHaveLength(1);
    expect(source).toMatch(/Data->SetNumberField\(TEXT\("componentsUpdated"\), Updated\);\s*Transaction\.DescribeInto\(Data\);\s*if \(!bMany\)/u);
  });

  it('set_visibility and set_actor_collision build their undo sets through the one helper, which flags what lacks RF_Transactional', () => {
    const undoSet = sliceBetween(support(), 'inline void McpAddActorUndoSet(', 'struct FMcpMotionInput');

    expect(visibility()).toContain('McpAddActorUndoSet(Actor, Undoable);');
    expect(collision()).toContain('McpAddActorUndoSet(Actor, Undoable);');
    expect(undoSet).toMatch(/if \(!Object->HasAnyFlags\(RF_Transactional\)\) \{\s*Object->SetFlags\(RF_Transactional\);\s*\}\s*Undoable\.Add\(Object\);/u);
    expect(support()).not.toContain('McpAddVisibilityUndoSet');
  });

  it('the record says the many form is one undo step', () => {
    const input = capabilityIndex().byId.get('control_actor.set_actor_collision')?.schemas.input;
    const actorNames = isRecord(input?.properties) ? input.properties.actorNames : undefined;

    expect(isRecord(actorNames) ? actorNames.description : '').toMatch(/one call and one undo step/u);
  });
});

// The receipt lists a call's changes from its reply: a single form names actorName and actorPath, but the
// actorNames form named no actor, so its receipt's changes[] came back empty although updatedActors, missing
// and the undo block were right. Both doors' extractors already read an affectedActors array.
describe('the many forms of set_visibility, set_actor_collision, add_tag and set_material name the actors they changed, for the receipt', () => {
  const from = (file: string, marker: string): string => {
    const source = read(file);
    return source.slice(source.indexOf(marker));
  };
  const addTag = (): string => sliceBetween(read('McpAutomationBridge_ControlActorTags.cpp'), 'HandleControlActorAddTag(', 'HandleControlActorRemoveTag(');
  const visibility = (): string => from('McpAutomationBridge_ControlActorTransform.cpp', 'HandleControlActorSetVisibility(');
  const collision = (): string => from('McpAutomationBridge_ControlActorPhysics.cpp', 'HandleControlActorSetCollision(');
  const nativeOutcome = readFileSync(
    join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP', 'Execute', 'McpNativeReceiptOutcome.cpp'),
    'utf8',
  );

  it('set_visibility lists the actors that read back as asked, each by name, in the many form only', () => {
    const source = visibility();
    const many = sliceBetween(source, 'if (bMany) {', '} else {');

    expect(many).toContain('Data->SetNumberField(TEXT("updatedActors"), Affected.Num());');
    expect(many).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(source.match(/TEXT\("affectedActors"\)/gu)).toHaveLength(1);
  });

  it('set_actor_collision lists the actors with a primitive component it updated, and updatedActors counts that same list', () => {
    const source = collision();
    const single = sliceBetween(source, 'if (!bMany)', 'Data->SetNumberField(TEXT("updatedActors")');

    expect(source).toMatch(/if \(OnActor == 0\)\s*\{\s*NoComponent\.Add\(McpActorRef\(Actor\)\);\s*\}\s*else\s*\{\s*Affected\.Add\(McpActorRef\(Actor\)\);\s*\}/u);
    expect(source).toContain('Data->SetNumberField(TEXT("updatedActors"), Affected.Num());');
    expect(source).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(single, 'the single form returns before the many branch').not.toContain('affectedActors');
  });

  it('add_tag lists every actor it resolved and tagged, each by name, in the many form only (remove_tag has no many form)', () => {
    const source = addTag();
    const single = source.slice(source.indexOf('if (TargetName.IsEmpty() || TagValue.IsEmpty())'));

    expect(source).toMatch(/TArray<FString> Affected;\s*for \(AActor \*Actor : Actors\) \{\s*Actor->Tags\.AddUnique\(TagName\);\s*Actor->MarkPackageDirty\(\);\s*Affected\.Add\(McpActorRef\(Actor\)\);\s*\}/u);
    expect(source).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(single, 'the single form returns after the many branch').not.toContain('affectedActors');
    expect(read('McpAutomationBridge_ControlActorTags.cpp').match(/TEXT\("affectedActors"\)/gu)).toHaveLength(1);
  });

  // set_material's many form runs the one-actor handler per name under a captured reply and answered only
  // {results, applied}, so its receipt had no change and no handle.
  it('set_material lists the actors that took the material, each by the name its one-actor run replied, once each', () => {
    const source = read('McpAutomationBridge_ControlActorMaterials.cpp');
    const many = sliceBetween(source, 'TEXT("actorNames")', 'FString TargetName;');

    expect(many).toMatch(/TArray<FString> Failures;\s*TArray<FString> Affected;\s*for \(int32 Index = 0;/u);
    expect(many).toMatch(
      /if \(Reply\.bSuccess\) \{\s*FString Changed;\s*if \(!Reply\.Result\.IsValid\(\) \|\| !Reply\.Result->TryGetStringField\(TEXT\("actorName"\), Changed\) \|\| Changed\.IsEmpty\(\)\) \{\s*Changed = Name;\s*\}\s*Affected\.AddUnique\(Changed\);\s*\} else \{/u
    );
    expect(many).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(many.indexOf('TEXT("affectedActors")'), 'the list rides on the incomplete reply too').toBeLessThan(many.indexOf('if (Failures.Num() > 0)'));
    expect(source.match(/TEXT\("affectedActors"\)/gu), 'the single form names actorName instead').toHaveLength(1);
  });

  it('a reply shaped like the many form lists the changed actors, and only them, as the receipt\'s changes on both doors', () => {
    expect(extractChanges({ success: true, visible: false, updatedActors: 2, missing: ['Gone'], affectedActors: ['Sign_1', 'Sign_2'], undo: { undoable: true } }))
      .toEqual(['Sign_1', 'Sign_2']);
    expect(extractChanges({ success: true, tag: 'Pickup', taggedCount: 2, missing: ['Gone'], affectedActors: ['Sign_1', 'Sign_2'], undo: { undoable: true } }))
      .toEqual(['Sign_1', 'Sign_2']);
    expect(nativeOutcome).toMatch(/CHANGE_ARRAYS\[\] = \{[^}]*TEXT\("affectedActors"\)/u);
  });

  it('the records say where the changed actors come back', () => {
    for (const id of ['control_actor.set_visibility', 'control_actor.set_actor_collision', 'control_actor.add_tag', 'control_actor.set_material']) {
      const properties = capabilityIndex().byId.get(id)?.schemas.input.properties;
      const actorNames = isRecord(properties) ? properties.actorNames : undefined;

      expect(isRecord(actorNames) ? actorNames.description : '', id).toMatch(/the actors (?:changed|tagged) under affectedActors/u);
    }
  });
});

// spawn_batch, the set_transform batch and the set_blueprint_variables batch answered a per-item results list and
// named no actor at the top, so a receipt listed nothing for a call that placed, moved or configured dozens. The
// single set_blueprint_variables answered only {updated} inside its data envelope, naming no actor either.
describe('the batch forms of spawn_batch, set_transform and set_blueprint_variables name the actors they changed, for the receipt', () => {
  const batch = (file: string, start: string, end: string): string => sliceBetween(read(file), start, end);

  it('spawn_batch lists every actor that spawned, by the name its result carries, under either report mode', () => {
    const source = read('McpAutomationBridge_ControlActorSpawnBatch.cpp');

    expect(source).toMatch(/const FString Shown = Actor \? \(bNamed \? Actor->GetActorLabel\(\) : Actor->GetName\(\)\) : ActorPath;\s*Affected\.Add\(Shown\);\s*if \(Actor\) \{\s*Entry->SetStringField\(TEXT\("name"\), Shown\);/u);
    expect(source.indexOf('Affected.Add(Shown);'), 'only an item that spawned is listed').toBeGreaterThan(source.indexOf('++SpawnedCount;'));
    expect(source).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(source.indexOf('TEXT("affectedActors")'), 'the report filter narrows results, not this list').toBeGreaterThan(source.indexOf('Results.RemoveAll('));
    expect(source.match(/TEXT\("affectedActors"\)/gu)).toHaveLength(1);
  });

  it('the set_transform batch lists each actor that moved once, by McpActorRef, in the pass that re-checks the layout', () => {
    const many = batch('McpAutomationBridge_ControlActorTransform.cpp', 'TEXT("actors")', 'FString TargetName;');

    expect(many).toMatch(/TArray<FString> Affected;\s*for \(const TSharedPtr<FJsonValue> &Value : Results\) \{[\s\S]*?if \(!Moved\)\s*continue;\s*Affected\.AddUnique\(McpActorRef\(Moved\)\);/u);
    expect(many).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(read('McpAutomationBridge_ControlActorTransform.cpp').match(/TEXT\("affectedActors"\)/gu), 'the set_visibility list and this one').toHaveLength(2);
  });

  it('the set_blueprint_variables batch lists an actor that took a variable, and the single form names the instance it changed', () => {
    const source = read('McpAutomationBridge_ControlActorAdvanced.cpp');
    const many = sliceBetween(source, 'TEXT("actors")', 'FString TargetName;');
    const single = source.slice(source.indexOf('FString TargetName;'), source.indexOf('HandleControlActorExport('));

    expect(many).toMatch(/if \(Updated && Updated->Num\(\) > 0\) \{\s*FString Changed;\s*if \(!ReplyData \|\| !\(\*ReplyData\)->TryGetStringField\(TEXT\("actorName"\), Changed\) \|\| Changed\.IsEmpty\(\)\)\s*Changed = Name;\s*Affected\.AddUnique\(Changed\);\s*\}/u);
    expect(many).toContain('Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));');
    expect(single).toMatch(/if \(Applied\.Num\(\) > 0\) \{[\s\S]*?Data->SetArrayField\(TEXT\("updated"\), AppliedArray\);\s*Data->SetStringField\(TEXT\("actorName"\), McpActorRef\(Found\)\);\s*\}/u);
    expect(single.match(/TEXT\("actorName"\), McpActorRef/gu), 'the actor is named only when a variable was set').toHaveLength(1);
  });

  it('replies shaped like the three batches give the receipt a change and an actor handle per actor, and nothing from the per-item results', () => {
    const spawned = { success: true, results: [{ index: 0, success: true, name: 'Block_1', path: '/Temp/Untitled_1.Untitled_1:PersistentLevel.Block_1' }], spawned: 2, failed: 0, affectedActors: ['Block_1', 'Block_2'], unnamedActors: ['StaticMeshActor_3'] };
    const moved = { success: true, results: [{ actorName: 'Sign_1', success: true, location: [0, 0, 1] }], movedActors: 2, affectedActors: ['Sign_1', 'Sign_2'] };
    const configured = { success: true, results: [{ actorName: 'Sign_1', success: true, updated: ['Headline'] }], updatedActors: 2, affectedActors: ['Sign_1', 'Sign_2'] };
    const single = { success: true, data: { updated: ['Headline'], actorName: 'Sign_1' }, warnings: [], error: null };

    expect(extractChanges(spawned)).toEqual(['Block_1', 'Block_2']);
    expect(extractHandles(spawned)).toEqual([{ kind: 'actor', ref: 'Block_1' }, { kind: 'actor', ref: 'Block_2' }]);
    for (const reply of [moved, configured]) {
      expect(extractChanges(reply)).toEqual(['Sign_1', 'Sign_2']);
      expect(extractHandles(reply)).toEqual([{ kind: 'actor', ref: 'Sign_1' }, { kind: 'actor', ref: 'Sign_2' }]);
    }
    expect(extractChanges(single)).toEqual(['Sign_1']);
    expect(extractHandles(single)).toEqual([{ kind: 'actor', ref: 'Sign_1' }]);
  });

  it('the records say where the changed actors come back', () => {
    for (const id of ['control_actor.spawn', 'control_actor.set_transform']) {
      const properties = capabilityIndex().byId.get(id)?.schemas.output.properties;
      const affected = isRecord(properties) ? properties.affectedActors : undefined;

      expect(isRecord(affected) ? affected.description : '', id).toMatch(/receipt lists them as changes, with an actor handle each/u);
    }
    const actors = capabilityIndex().byId.get('control_actor.set_blueprint_variables')?.schemas.input.properties;
    const description = isRecord(actors) && isRecord(actors.actors) ? actors.actors.description : '';

    expect(description).toMatch(/the actors that took a variable come back under affectedActors/u);
  });
});

// set_material changed components with Modify() and no transaction around it, so nothing it did reached the
// undo buffer and the reply carried no `undo` block, where set_visibility and set_actor_collision name theirs.
describe('set_material is one undoable step, single and many, as set_visibility is', () => {
  const source = (): string => read('McpAutomationBridge_ControlActorMaterials.cpp');
  const many = (): string => sliceBetween(source(), 'TEXT("actorNames")', 'FString TargetName;');
  const single = (): string => source().slice(source().indexOf('FString TargetName;'));
  const registry = readFileSync(
    join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Core', 'Requests', 'McpResponseCaptureRegistry.cpp'),
    'utf8',
  );
  const registryHeader = readFileSync(
    join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Core', 'Requests', 'McpResponseCaptureRegistry.h'),
    'utf8',
  );

  it('the single form settles which components take the slot before it opens the transaction, so a no-op leaves no undo step', () => {
    const body = single();

    expect(body).toMatch(/TArray<UPrimitiveComponent \*> Takers;\s*for \(UPrimitiveComponent \*Component : TargetComponents\) \{\s*if \(Component && MaterialSlot < Component->GetNumMaterials\(\)\) \{\s*Takers\.Add\(Component\);\s*if \(!bAllComponents\) \{\s*break;\s*\}/u);
    expect(body.indexOf('TEXT("MATERIAL_SLOT_NOT_FOUND")'), 'refused before anything is recorded').toBeLessThan(body.indexOf('MakeUnique<FMcpScopedEditorTransaction>'));
    expect(body.indexOf('MakeUnique<FMcpScopedEditorTransaction>'), 'opened before the first Modify').toBeLessThan(body.indexOf('Component->Modify();'));
    expect(body).toContain('for (UPrimitiveComponent *Component : Takers) {');
    expect(body, 'the loop no longer re-checks what Takers settled').not.toContain('continue;');
  });

  it('the single form opens a "Set Actor Material" transaction over the actor and its primitive components, and describes it in the reply', () => {
    const body = single();

    expect(body).toMatch(/if \(!FMcpResponseCaptureRegistry::Get\(\)\.IsCapturing\(RequestId\)\) \{\s*TArray<UObject \*> Undoable;\s*McpAddActorUndoSet\(Found, Undoable\);\s*Transaction = MakeUnique<FMcpScopedEditorTransaction>\(\s*FText::FromString\(TEXT\("Set Actor Material"\)\), EMcpMutationDurability::EditorStateOnly, Undoable\);\s*\}/u);
    expect(body).toMatch(/Data->SetArrayField\(TEXT\("components"\), AppliedComponents\);\s*if \(Transaction\) \{\s*Transaction->DescribeInto\(Data\);\s*\}\s*McpHandlerUtils::AddVerification\(Data, Found\);/u);
  });

  it('the actorNames form holds one transaction over every actor that resolves, and describes it on the incomplete reply too', () => {
    const body = many();

    expect(body).toMatch(/TArray<UObject \*> Undoable;\s*for \(const TSharedPtr<FJsonValue> &Named : \*Names\) \{\s*if \(AActor \*Actor = Named\.IsValid\(\) \? FindActorByName\(Named->AsString\(\)\) : nullptr\) \{\s*McpAddActorUndoSet\(Actor, Undoable\);\s*\}\s*\}\s*TUniquePtr<FMcpScopedEditorTransaction> Transaction;\s*if \(Undoable\.Num\(\) > 0\) \{\s*Transaction = MakeUnique<FMcpScopedEditorTransaction>\(\s*FText::FromString\(TEXT\("Set Actor Material"\)\), EMcpMutationDurability::EditorStateOnly, Undoable\);\s*\}/u);
    expect(body.indexOf('MakeUnique<FMcpScopedEditorTransaction>'), 'opened before the first actor runs').toBeLessThan(body.indexOf('HandleControlActorSetMaterial(ItemId, One, Socket);'));
    expect(body).toMatch(/if \(Transaction\) \{\s*Transaction->DescribeInto\(Data\);\s*\}\s*if \(Failures\.Num\(\) > 0\) \{/u);
  });

  it('a run inside a batch leaves the step to its caller: the list runs every actor captured, and so does spawn_batch\'s material', () => {
    expect(many()).toMatch(/Capture\.Begin\(ItemId\);\s*HandleControlActorSetMaterial\(ItemId, One, Socket\);/u);
    expect(read('McpAutomationBridge_ControlActorSpawnBatch.cpp')).toMatch(/Capture\.Begin\(MaterialId\);\s*HandleControlActorSetMaterial\(MaterialId, MaterialPayload, Socket\);/u);
    expect(registryHeader).toContain('bool IsCapturing(const FString& RequestId) const;');
    expect(registryHeader).toContain('mutable FCriticalSection Mutex;');
    expect(registry).toMatch(/bool FMcpResponseCaptureRegistry::IsCapturing\(const FString& RequestId\) const\s*\{\s*FScopeLock Lock\(&Mutex\);\s*return Pending\.Contains\(RequestId\);\s*\}/u);
  });

  it('the record says the list is one undo step', () => {
    const properties = capabilityIndex().byId.get('control_actor.set_material')?.schemas.input.properties;
    const actorNames = isRecord(properties) ? properties.actorNames : undefined;

    expect(isRecord(actorNames) ? actorNames.description : '').toMatch(/one call and one undo step/u);
  });
});

// find had findBy class and name only: the actors drawing one mesh, or showing one material in any slot, could not be asked for.
describe('control_actor find asks which actors use a static mesh or a material', () => {
  const find = (): string => read('Find', 'McpAutomationBridge_FindByAsset.cpp');
  const record = () => {
    const found = capabilityIndex().byId.get('control_actor.find');
    if (found === undefined) throw new Error('control_actor.find is not in the catalogue');
    return found;
  };

  it('findBy takes mesh and material beside class and name, and the old names run them too', () => {
    const dispatchBy = record()?.routing.dispatchBy;

    expect(dispatchBy?.actions).toEqual({ class: 'find_by_class', name: 'find_by_name', mesh: 'find_by_mesh', material: 'find_by_material' });
    expect(dispatchBy?.declaredBy?.meshPath).toEqual(['mesh']);
    expect(dispatchBy?.declaredBy?.materialPath).toEqual(['material']);
    expect(dispatchBy?.declaredBy?.limit, 'limit is read by the two asset finds only').toEqual(['mesh', 'material']);
    const folded = Object.fromEntries((record()?.legacyIds ?? []).map((entry) => [entry.action, entry.folded]));
    expect(folded.find_by_mesh).toEqual({ findBy: 'mesh' });
    expect(folded.find_by_material).toEqual({ findBy: 'material' });
    expect(unreadVariantParams(record(), ['limit'], { findBy: 'class' })).toHaveLength(1);
    expect(unreadVariantParams(record(), ['limit', 'meshPath'], { findBy: 'mesh' })).toEqual([]);
  });

  it('the schema takes a path and a limit for each, and refuses an empty limit or a key nothing reads', () => {
    const input = record()?.schemas.input;
    const mesh = { action: 'find', findBy: 'mesh', meshPath: '/Engine/BasicShapes/Cube' };

    expect(validateAgainstCapabilitySchema(mesh, input)).toBeUndefined();
    expect(validateAgainstCapabilitySchema({ action: 'find', findBy: 'material', materialPath: '/Game/Materials/M_Rock', limit: 5 }, input)).toBeUndefined();
    expect(validateAgainstCapabilitySchema({ ...mesh, limit: 0 }, input)?.pointer).toBe('/limit');
    expect(validateAgainstCapabilitySchema({ ...mesh, bogus: 1 }, input)).toBeDefined();
  });

  it('the dispatcher sends both to their handlers, which live in their own folder', () => {
    const dispatch = read('McpAutomationBridge_ControlActorDispatch.cpp');

    expect(dispatch).toContain('#include "Domains/ControlActor/Find/McpAutomationBridge_FindByAsset.h"');
    expect(dispatch).toMatch(/LowerSub == TEXT\("find_by_mesh"\)\)\s*return McpFindByAsset::HandleFindByMesh\(this, RequestId, Payload, RequestingSocket\);/u);
    expect(dispatch).toMatch(/LowerSub == TEXT\("find_by_material"\)\)\s*return McpFindByAsset::HandleFindByMaterial\(this, RequestId, Payload, RequestingSocket\);/u);
  });

  it('the scan reads the world find_by_class reads, answers count and actors, and says when the limit cut the list', () => {
    const source = find();

    expect(source).toMatch(/UWorld\* World = GEditor->PlayWorld \? GEditor->PlayWorld\.Get\(\) : GEditor->GetEditorWorldContext\(\)\.World\(\);/u);
    expect(source).toContain('constexpr int32 DefaultLimit = 200;');
    expect(source).toContain('constexpr int32 MaxLimit = 1000;');
    expect(source).toMatch(/Payload->TryGetNumberField\(TEXT\("limit"\), Requested\)\s*\? FMath::Clamp\(static_cast<int32>\(Requested\), 1, MaxLimit\) : DefaultLimit;/u);
    expect(source).toContain('Data->SetNumberField(TEXT("count"), Actors.Num());');
    expect(source).toContain('Data->SetArrayField(TEXT("actors"), Actors);');
    expect(source).toMatch(/if \(Total > Actors\.Num\(\)\)\s*\{\s*Data->SetBoolField\(TEXT\("truncated"\), true\);\s*Data->SetNumberField\(TEXT\("totalCount"\), Total\);\s*\}/u);
    for (const field of ['label', 'name', 'path', 'class', 'components']) {
      expect(source, field).toContain(`Row->Set${field === 'components' ? 'Array' : 'String'}Field(TEXT("${field}")`);
    }
  });

  it('a mesh matches by what a StaticMeshComponent draws, instanced and foliage components included, and says how many instances', () => {
    const mesh = sliceBetween(find(), 'bool HandleFindByMesh(', 'bool HandleFindByMaterial(');

    expect(mesh).toMatch(/const UStaticMeshComponent\* MeshComponent = Cast<UStaticMeshComponent>\(Component\);\s*if \(!MeshComponent \|\| MeshComponent->GetStaticMesh\(\) != Mesh\)\s*\{\s*return false;\s*\}/u);
    expect(mesh).toContain('Cast<UInstancedStaticMeshComponent>(Component)');
    expect(mesh).toContain('Entry->SetNumberField(TEXT("instances"), Instanced->GetInstanceCount());');
    expect(mesh).toMatch(/Cast<UStaticMesh>\(McpLoadAsset\(SafePath\)\)/u);
  });

  it('a material matches by the slots that use it, an override or the mesh default, looking through a dynamic instance', () => {
    const source = find();
    const material = source.slice(source.indexOf('bool HandleFindByMaterial('));

    expect(material).toMatch(/for \(int32 Slot = 0; Slot < Component->GetNumMaterials\(\); \+\+Slot\)\s*\{\s*if \(AssetBehind\(Component->GetMaterial\(Slot\)\) == Material\)/u);
    expect(material).toContain('Entry->SetArrayField(TEXT("slots"), Slots);');
    expect(material).toContain('LoadMaterialForMcp(MaterialPath, ResolvedPath, LoadError);');
    expect(source).toMatch(/while \(UMaterialInstanceDynamic\* Dynamic = Cast<UMaterialInstanceDynamic>\(Material\)\)\s*\{\s*Material = Dynamic->Parent;\s*\}/u);
  });

  it('a missing path or an asset that does not load is an error naming it, not an empty list', () => {
    const source = find();

    expect(source).toContain('TEXT("meshPath is required.")');
    expect(source).toContain('TEXT("materialPath is required.")');
    expect(source.split('TEXT("INVALID_ARGUMENT")')).toHaveLength(3);
    expect(source).toContain('TEXT("MESH_NOT_FOUND")');
    expect(source).toContain('Bridge->SendAutomationError(Socket, RequestId, LoadError, TEXT("MATERIAL_NOT_FOUND"));');
  });
});

// A spawn ran outside any transaction, so after six actors were placed control_editor.undo answered NOTHING_TO_UNDO.
describe('every spawn is one undo step, "Spawn Actors", and the reply says so', () => {
  const support = (): string => read('McpAutomationBridge_ControlActorSupport.h');

  it('the helper opens it, and leaves it to the batch when the run is captured', () => {
    expect(support()).toMatch(/inline TUniquePtr<FMcpScopedEditorTransaction> McpBeginSpawnTransaction\(const FString &RequestId\) \{\s*if \(FMcpResponseCaptureRegistry::Get\(\)\.IsCapturing\(RequestId\)\) \{\s*return nullptr;\s*\}\s*return MakeUnique<FMcpScopedEditorTransaction>\(FText::FromString\(TEXT\("Spawn Actors"\)\),\s*EMcpMutationDurability::EditorStateOnly, TArray<UObject \*>\(\)\);\s*\}/u);
  });

  it('spawn and spawn_blueprint open it before the level is touched and describe it after the actor exists', () => {
    for (const [file, reply] of [['McpAutomationBridge_ControlActorSpawn.cpp', 'Data'], ['McpAutomationBridge_ControlActorBlueprintSpawn.cpp', 'Resp']] as const) {
      const source = read(file);
      const opened = source.indexOf('McpBeginSpawnTransaction(RequestId);');

      expect(opened, file).toBeGreaterThan(-1);
      expect(opened, `${file}: before the world is modified`).toBeLessThan(source.indexOf('TargetWorld->Modify();'));
      expect(opened, `${file}: before the spawn`).toBeLessThan(source.indexOf('TargetWorld->SpawnActor('));
      expect(source.split(/\s+/u).join(' '), file).toContain(`if (Transaction) { Transaction->DescribeInto(${reply}); }`);
      expect(source.indexOf(`Transaction->DescribeInto(${reply});`), `${file}: after the spawn`).toBeGreaterThan(source.indexOf('TargetWorld->SpawnActor('));
    }
  });

  it('spawn_batch holds one step for every item and describes it whether or not every item spawned', () => {
    const source = read('McpAutomationBridge_ControlActorSpawnBatch.cpp');
    const opened = source.indexOf('McpBeginSpawnTransaction(RequestId);');

    expect(opened).toBeGreaterThan(-1);
    expect(opened, 'before the first item runs').toBeLessThan(source.indexOf('Capture.Begin(SpawnId);'));
    expect(source).toMatch(/if \(Transaction\) \{\s*Transaction->DescribeInto\(Data\);\s*\}/u);
    expect(source.indexOf('Transaction->DescribeInto(Data);'), 'before either reply goes out').toBeLessThan(source.indexOf('SendAutomationResponse('));
  });

  it('the records declare the undo block of every spawn form', () => {
    for (const id of ['control_actor.spawn']) {
      const properties = capabilityIndex().byId.get(id)?.schemas.output.properties;
      const undo = isRecord(properties) ? properties.undo : undefined;

      expect(isRecord(undo) ? undo.description : '', id).toMatch(/undoable: true, transactionScope: "Spawn Actors"/u);
    }
  });
});
