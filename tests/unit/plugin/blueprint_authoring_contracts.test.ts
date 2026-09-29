// Wiring contracts of the Blueprint graph and member handlers that no unit test can
// reach by running them (they need an editor). Behaviour itself belongs to the
// integration cases in tests/mcp-tools/core/manage-blueprint*.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function read(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(PRIVATE, ...segments), 'utf8'));
}

function paramDescription(capability: string, param: string): string {
  const properties = capabilityIndex().byId.get(capability)?.schemas.input.properties;
  const entry = isRecord(properties) ? properties[param] : undefined;
  return isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';
}

describe('get_graph_details filter reads what a node\'s pins hold', () => {
  const queries = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersQueries.cpp');

  it('a node matches by title or name, a pin default value, a text default or a default object path', () => {
    const helper = queries().slice(queries().indexOf('NodeMatchesGraphFilter('));

    expect(helper).toMatch(/Matches\(Title\) \|\| Node->GetName\(\)\.Contains\(SquashedFilter\)/u);
    expect(helper).toMatch(/Matches\(Pin->DefaultValue\)/u);
    expect(helper).toMatch(/Matches\(Pin->DefaultTextValue\.ToString\(\)\)/u);
    expect(helper).toMatch(/Pin->DefaultObject && Matches\(Pin->DefaultObject->GetPathName\(\)\)/u);
    expect(helper).toMatch(/Text\.Replace\(TEXT\(" "\), TEXT\(""\)\)\.Contains\(SquashedFilter\)/u);
  });

  it('the listing loop filters through it, so the paging counts agree', () => {
    expect(queries()).toMatch(/if \(!Filter\.IsEmpty\(\) && !NodeMatchesGraphFilter\(Node, Title, SquashedFilter\)\)/u);
  });

  it('the filter description says so', () => {
    const description = paramDescription('blueprint.inspect_graph', 'filter');

    expect(description).toMatch(/pin default value/u);
    expect(description).toMatch(/default object path/u);
    expect(description).toMatch(/WBP_MainMenu/u);
  });
});

describe('variableType: the description lists what the type resolver accepts', () => {
  const baseTypes = (): string => read('Foundation', 'Blueprint', 'McpBlueprintUtilsBaseTypes.cpp');
  const resolver = (): string => read('Foundation', 'Blueprint', 'McpBlueprintUtilsTypeResolver.cpp');
  const description = (): string => paramDescription('blueprint.edit_variable', 'variableType');

  it('names only basic types the resolver knows, and a class path as an object reference', () => {
    const known = new Set([...baseTypes().matchAll(/TEXT\("(\w+)"\)/gu)].map((match) => (match[1] ?? '').toLowerCase()));
    const basics = /Basic: ([^.]+)\./u.exec(description())?.[1]?.split(', ') ?? [];

    expect(basics.length).toBeGreaterThan(10);
    for (const name of basics) expect(known.has(name.toLowerCase()), `${name} is a basic type`).toBe(true);
    expect(description()).toMatch(/a bare class name or path is an object reference/u);
    expect(baseTypes()).toMatch(/ResolveClassByName\(Token\)\)\s*\{\s*OutPin = MakePin\(K2::PC_Object, NAME_None, ClassResolve\);/u);
  });

  it('documents the class, struct, enum and container spellings it parses, with one example', () => {
    for (const prefix of ['object:', 'class:', 'softobject:', 'softclass:', 'enum:', 'struct:']) {
      expect(baseTypes(), prefix).toContain(`TEXT("${prefix}")`);
    }
    for (const spelling of ['Object:<class>', 'Class:<class>', 'SoftObject:<class>', 'SoftClass:<class>', 'struct:<path>', 'enum:<object path>',
      'Array<T>', 'Set<T>', 'Map<Key,Value>', 'Array:T', 'Set:T', 'Map:Key,Value']) {
      expect(description(), spelling).toContain(spelling);
    }
    for (const container of ['array:', 'set:', 'map:', 'array<', 'set<', 'map<']) {
      expect(resolver(), container).toContain(`TEXT("${container}")`);
    }
    expect(description()).toContain('Example: Array<Object:/Script/UMG.Widget>');
  });

  it('the batch add_variable step points at the same specs', () => {
    expect(paramDescription('blueprint.edit_graph', 'operations')).toMatch(/variableType as add_variable takes it/u);
  });
});

describe('build_graph: an overlapping explicit position moves the node instead of stopping the batch', () => {
  const placement = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp');

  it('walks the overlap guard\'s suggestions for many hops, then the auto grid; the old six-retry cap is gone', () => {
    const source = placement();

    expect(source).toMatch(/constexpr int32 MaxSuggestionHops = 24;/u);
    expect(source).toMatch(/constexpr int32 MaxPlacementTries = 48;/u);
    expect(source).toMatch(/Moves < MaxSuggestionHops\)\s*\{\s*Payload->SetNumberField\(TEXT\("posX"\), \(\*Suggested\)->GetNumberField\(TEXT\("x"\)\)\);/u);
    expect(source).toMatch(/else\s*\{\s*PlaceOnGrid\(State, Payload\);\s*\}/u);
    expect(source).not.toMatch(/Retry < 6/u);
  });

  it('answers only NODE_OVERLAP refusals; any other failure still stops the batch', () => {
    const source = placement();

    expect(source).toMatch(/Reply\.ErrorCode == TEXT\("NODE_OVERLAP"\)/u);
    expect((source.match(/TEXT\("NODE_OVERLAP"\)/gu) ?? []).length).toBe(1);
  });

  it('tells a caller who named the position where the node went, in the step\'s placementWarning', () => {
    const source = placement();

    expect(source).toMatch(/if \(Moves > 0 && !bAuto && Reply\.bSuccess && Reply\.Result\.IsValid\(\)\)/u);
    expect(source).toMatch(/Requested position \(%d, %d\) overlaps %s; the node was placed at \(%d, %d\) instead\./u);
    expect(source).toMatch(/Reply\.Result->SetStringField\(TEXT\("placementWarning"\), Warning\);/u);
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchSteps.cpp')).toContain('TEXT("placementWarning")');
  });

  it('the batch description says so, and where a step\'s placementWarning appears', () => {
    expect(paramDescription('blueprint.edit_graph', 'operations')).toMatch(/overlap an existing node is placed at the nearest free position/u);
  });
});

describe('a VariableGet of a widget: compile once when the class is stale, and never claim a set flag is false', () => {
  const source = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersVariableNodes.cpp');

  it('a flagged widget with no property compiles the Blueprint once and looks again, but not during play', () => {
    expect(source()).toMatch(
      /if \(!FoundProperty && !bFoundAsBlueprintVariable && TreeWidget && TreeWidget->bIsVariable &&\s*!\(GEditor && GEditor->PlayWorld\)\)\s*\{\s*McpSafeCompileBlueprint\(Context\.Blueprint\);\s*bCompiledHere = true;/u
    );
    expect(source()).toMatch(/GeneratedClass->FindPropertyByName\(VariableFName\)/u);
  });

  it('"not marked as a variable" is said only when the flag really is false', () => {
    const text = source();
    const flagged = text.slice(text.indexOf('if (TreeWidget && TreeWidget->bIsVariable)'), text.indexOf('else if (TreeWidget)'));
    const unflagged = text.slice(text.indexOf('else if (TreeWidget)'), text.indexOf('Variable \'%s\' not found'));

    expect(flagged).toContain('is marked as a variable');
    expect(flagged).not.toContain('not marked as a variable');
    expect(unflagged).toContain('is not marked as a variable');
  });
});

describe('add_event with componentName binds a Widget Blueprint widget\'s event as it binds a component\'s', () => {
  const bound = (): string => read('Domains', 'Blueprint', 'Events', 'McpAutomationBridge_BlueprintHandlersAddEventComponentBound.cpp');

  it('a widget variable of the generated class is a bindable object property beside the components', () => {
    const source = bound();

    expect(source).toMatch(
      /IsBindable = \[\]\(const UClass \*PropertyClass\)\s*\{\s*return PropertyClass && \(PropertyClass->IsChildOf\(UActorComponent::StaticClass\(\)\) \|\|\s*PropertyClass->IsChildOf\(UWidget::StaticClass\(\)\)\);/u
    );
    expect(source).toMatch(/Equals\(ComponentName, ESearchCase::IgnoreCase\) && IsBindable\(PropIt->PropertyClass\)/u);
    expect(source).toContain('#include "Components/Widget.h"');
  });

  it('the refusal lists the widget variables too, and compile-once-before-giving-up is kept', () => {
    const source = bound();

    expect(source).toContain('Its components and widget variables: %s.');
    expect(source).toMatch(/if \(!ComponentProp\) \{\s*McpSafeCompileBlueprint\(BP\);\s*ComponentProp = FindComponentProperty\(\);/u);
  });

  it('the add_event record and the batch description name the widget case', () => {
    expect(paramDescription('blueprint.add_function', 'componentName')).toMatch(/Widget Blueprint a widget of its tree that is a variable/u);
    expect(paramDescription('blueprint.add_function', 'eventName')).toMatch(/OnClicked/u);
    expect(paramDescription('blueprint.edit_graph', 'operations')).toMatch(/component or Widget Blueprint widget delegate/u);
  });
});

describe('an edit_graph reply names the Blueprint it ran on', () => {
  const shared = (): string => read('Domains', 'BlueprintGraph', 'Context', 'McpAutomationBridge_BlueprintGraphHandlersContextShared.cpp');
  const batch = (): string => read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp');

  it('NameBlueprint writes blueprintPath unless an assetPath is there, and changedAssets for a change', () => {
    const body = shared().slice(shared().indexOf('void FActionContext::NameBlueprint('), shared().indexOf('void FActionContext::SendResponse('));

    expect(body).toMatch(/Blueprint->GetOutermost\(\)->GetName\(\)/u);
    expect(body).toMatch(/!Result->HasField\(TEXT\("assetPath"\)\) && !Result->HasField\(TEXT\("blueprintPath"\)\)/u);
    expect(body).toMatch(/bChanged && !Result->HasField\(TEXT\("changedAssets"\)\)/u);
    expect(body).toMatch(/SetArrayField\(TEXT\("changedAssets"\), Changed\)/u);
  });

  it('every reply that goes through the funnel is named, on the dirty test the funnel already uses', () => {
    const send = shared().slice(shared().indexOf('void FActionContext::SendResponse('));

    expect(send).toMatch(/NameBlueprint\(Result, Blueprint && Blueprint->Status == BS_Dirty\);/u);
    expect(send.indexOf('NameBlueprint(')).toBeLessThan(send.indexOf('McpCompileBlueprintWithDiagnostics('));
  });

  it('the batch states its own change, since its compile leaves the Blueprint clean', () => {
    const source = batch();

    expect(source).toMatch(/Context\.NameBlueprint\(Result,\s*true\);/u);
    expect(source.indexOf('Context.NameBlueprint(')).toBeLessThan(source.indexOf('McpCompileBlueprintWithDiagnostics('));
  });
});
