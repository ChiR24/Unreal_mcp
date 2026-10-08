// Wiring contracts of the Blueprint graph and member handlers that no unit test can
// reach by running them (they need an editor). Behaviour itself belongs to the
// integration cases in tests/mcp-tools/core/manage-blueprint*.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { inferSelector, unreadVariantParams } from '../../../src/server/gateway/gateway-dispatch-by.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

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

describe('a function the memberClass lacks names the classes that do declare it', () => {
  it('searches every loaded class, not only the function libraries', () => {
    const search = sliceBetween(
      read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlers.cpp'),
      'FString DescribeDeclaringClasses(',
      'bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(',
    );

    expect(search).toContain('It->FindFunctionByName(WantedName, EIncludeSuperFlag::ExcludeSuper)');
    expect(search).not.toContain('UBlueprintFunctionLibrary');
    expect(search).toContain("retry with memberClass '%s'");
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersPrivate.h'))
      .toContain('return DescribeDeclaringClasses(Class, Wanted);');
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

  // "Right of the pile" alone walked a node asked for beside BeginPlay along a crowded event row,
  // 24 piles, then to the graph's far right edge (x 46464).
  it('the overlap guard suggests the nearest free slot in any direction', () => {
    const layout = read('Foundation', 'GraphLayout', 'McpGraphNodeExtent.h');
    expect(layout).toContain('inline bool FindNearestFreeSlot(');
    expect(layout).toMatch(/FindNearestFreeSlot\(Graph, NewX, NewY, NewW, NewH, IgnoreNode, FreeX, FreeY\)/u);
    expect(layout).toContain('BuildNodeOverlapDetails(PosX, PosY, Width, Height, Overlapping, OutMessage, Graph)');
  });
});

// create_node MakeStruct in a batch (structPath given) made a pinless "Make <unknown struct>" and
// the batch answered success.
describe('create_node: a Make/Break struct node gets its struct or is refused', () => {
  it('sets StructType from structPath before the pins are allocated', () => {
    const dynamic = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersNodeCreationDynamic.cpp');
    expect(dynamic).toContain('NodeClass->IsChildOf(UK2Node_StructOperation::StaticClass())');
    expect(dynamic).toMatch(/needs structPath: a Blueprint Struct asset/u);
    expect(dynamic.indexOf('StructNode->StructType = StructType;')).toBeGreaterThan(-1);
    expect(dynamic.indexOf('StructNode->StructType = StructType;')).toBeLessThan(dynamic.indexOf('NewNode->AllocateDefaultPins();'));
  });

  it('the create_node contract declares structPath', () => {
    expect(paramDescription('blueprint.edit_graph', 'structPath')).toMatch(/For MakeStruct or BreakStruct|Blueprint Struct asset path/u);
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

  // A widget edit saves but leaves the Blueprint uncompiled (dirty), and the next inspect_graph claimed the change.
  it('every reply that goes through the funnel is named, on the dirty test the funnel already uses; a read claims no change', () => {
    const send = shared().slice(shared().indexOf('void FActionContext::SendResponse('));

    expect(send).toMatch(/NameBlueprint\(Result, !bDeferCompile && Blueprint && Blueprint->Status == BS_Dirty\);/u);
    expect(send.indexOf('NameBlueprint(')).toBeLessThan(send.indexOf('McpCompileBlueprintWithDiagnostics('));
  });

  it('the batch states its own change, since its compile leaves the Blueprint clean', () => {
    const source = batch();

    expect(source).toMatch(/Context\.NameBlueprint\(Result,\s*true\);/u);
    expect(source.indexOf('Context.NameBlueprint(')).toBeLessThan(source.indexOf('McpCompileBlueprintWithDiagnostics('));
  });
});

// "Which Blueprints show a TextRender label" took one get_scs per Blueprint.
describe('get_scs scans a folder for a component class', () => {
  const source = read('Domains', 'Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersScsGet.cpp');

  it('takes path without blueprintPath as a folder scan, capped and sorted', () => {
    expect(source).toMatch(/if \(!Folder\.IsEmpty\(\) && RequestedBlueprint\.IsEmpty\(\)\) \{\s*return SendScsFolderScan\(/u);
    expect(source).toContain('NormalizeAssetPath(Folder)');
    expect(source).toContain('GetAssetsByPath(FName(*Norm.Path), Assets, true)');
    expect(source).toContain('bTruncated = Scanned >= McpScsScanMaxBlueprints;');
  });

  it('matches a component class through its parents, and filters one Blueprint too', () => {
    expect(source).toMatch(/Class = Class->GetSuperClass\(\)/u);
    expect(source).toContain('ComponentClassMatches(Node->ComponentClass, Filter)');
    expect(source).toContain('ComponentClassMatches(Comp->GetClass(), Filter)');
  });

  // A child Blueprint that adds no TextRender of its own still shows its parent's, and a
  // Character's Mesh comes from the native parent: the scan read only each Blueprint's own SCS.
  it('the scan and the listing both count inherited components', () => {
    expect(source).toContain('McpPropertyCdoComponents::ForEachScsNode(Blueprint');
    expect(source.match(/CollectInheritedComponents\(Blueprint, Filter, /gu)).toHaveLength(2);
    expect(source).toContain('#include "Core/Compatibility/McpVersionCompatibility.h"');
  });

  it('declares path and componentClass on the record', () => {
    expect(paramDescription('blueprint.get_scs', 'path')).toContain('folder');
    expect(paramDescription('blueprint.get_scs', 'path')).toContain('inherited');
    expect(paramDescription('blueprint.get_scs', 'componentClass')).toContain('TextRender');
  });
});

// Reading what an event does (Bounce, TakeHit, an enemy's stomp test) took one pin call per hop.
describe('get_node_details followExec lists what runs after a node', () => {
  const source = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersDetails.cpp');

  it('walks exec outputs only, once per node, capped at 50', () => {
    expect(source).toContain('Out->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec');
    expect(source).toContain('Seen.Contains(Next)');
    expect(source).toContain('McpExecChain(TargetNode, FMath::Min(static_cast<int32>(FollowExec), 50))');
  });

  it('gives each input its value or its source, and the record says so', () => {
    expect(source).toContain('TEXT("<- ")');
    expect(paramDescription('blueprint.inspect_graph', 'followExec')).toContain('chain');
  });
});

// A HitStop custom event made by an add_event step came back with "pins": "", so its Duration
// input had to be guessed before a later step could wire it.
describe('an add_event batch step lists the pins of the event it made', () => {
  it('looks the node up across the Blueprint when the step prepared no graph', () => {
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp'))
      .toContain('DescribeNodePins(Step.TargetGraph ? Step.FindNode(Guid) : FindBatchNode(Parent.Blueprint, Guid))');
  });
});

// A HitStop event added by a batch sat at (0, 0), over whatever the graph had at its origin.
describe('an add_event batch step without a position takes an auto grid slot', () => {
  it('places add_event like a create step', () => {
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp'))
      .toContain('(bCreates || Edit == TEXT("add_event")) && !Payload->HasField(TEXT("posX"))');
  });

  it('keeps hidden pins (a library call\'s self, LatentInfo) out of a chain\'s inputs', () => {
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersDetails.cpp')).toContain('In->bHidden');
  });
});

// A Cast step with targetClass /Game/MyGame/Blueprints/BP_MyGameInstance stopped a batch with
// "Class not found": the documented Blueprint path names the Blueprint, not its class.
describe('a Blueprint asset path resolves wherever a graph node takes a class', () => {
  it('Cast, VariableGet/Set memberClass and event memberClass fall back to the class-pin resolver', () => {
    const special = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersSpecialNodes.cpp');
    expect(special).toContain('TargetClass = ResolveTargetClassFromString(TargetClassName);');
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersVariableNodes.cpp'))
      .toContain('OwnerClass = ResolveTargetClassFromString(MemberClassName);');
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersFunctionEventNodes.cpp'))
      .toContain('TargetClass = ResolveTargetClassFromString(MemberClass);');
  });
});

// Auto-placed batch nodes sat on a grid right of the whole graph: a chain added to BP_Rider's Bounce event
// (x 7646) landed at x 46464, a wire across the graph.
describe('build_graph settles auto-placed nodes beside what they are wired to', () => {
  it('records each auto-placed node and settles them after the last step, before the compile', () => {
    const placement = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp');
    expect(placement).toContain('State.AutoPlacedGuids.Add(CreatedGuid)');
    expect(placement).toContain('FindNearestFreeSlot(Node->GetGraph(), Anchor.X, Anchor.Y, Width, Height, Node, X, Y, PreferDX)');
    const batch = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp');
    const settle = batch.indexOf('SettleAutoPlacedNodes(Context.Blueprint, State)');
    expect(settle).toBeGreaterThan(-1);
    expect(settle).toBeLessThan(batch.indexOf('McpCompileBlueprintWithDiagnostics'));
  });

  // A pure node settled before the Branch reading it went beside the variable it reads, 1640 units away.
  it('a node waits for the neighbour it belongs beside, and a stalled pass relaxes one node at a time', () => {
    const placement = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp');
    expect(placement).toContain('Best.bBetterPending = PendingRank < Best.Rank;');
    expect(placement).toContain('if (Anchor.bBetterPending && !bRelax)');
    expect(placement).toContain('Index < Pending.Num() && !(bRelax && bMoved)');
  });

  // In the HUD's zig-zag Tick chain, PlayAnimation run by a new Branch took the nearest free slot up-left of it.
  it('a taken spot is replaced on the anchor side, the other side only when that side has none nearby', () => {
    const placement = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchPlacement.cpp');
    expect(placement).toContain('const int32 PreferDX = Anchor.Rank == 0 || Anchor.Rank == 3 ? 1 : -1;');
    const layout = read('Foundation', 'GraphLayout', 'McpGraphNodeExtent.h');
    expect(layout).toContain('const UEdGraphNode* IgnoreNode, float& OutX, float& OutY, int32 PreferDX = 0)');
    expect(layout).toContain('FPick& Pick = PreferDX * DX < 0 ? Other : Same;');
    expect(layout).toContain('Ring >= FallbackRing + OtherSideGraceRings');
  });

  it('the batch description says where an auto-placed node goes', () => {
    expect(paramDescription('blueprint.edit_graph', 'operations')).toContain('moves beside a node it is wired to');
  });
});

describe('arrange_nodes moves listed nodes beside what they are wired to', () => {
  it('parks the listed nodes, settles them like auto-placed batch nodes, and puts back the ones that found no anchor', () => {
    const source = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersDeleteNodes.cpp');
    const park = source.indexOf('Node->NodePosY = -1000000;');
    const settle = source.indexOf('GraphBatch::SettleAutoPlacedNodes(Context.Blueprint, State)');
    expect(park).toBeGreaterThan(-1);
    expect(settle).toBeGreaterThan(park);
    expect(source).toContain('if (Unmoved.Contains(Node))');
    expect(read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp')).toContain('ArrangeNodes(Context)');
    expect(read('Core', 'Subsystem', 'McpAutomationBridgeSubsystemHandlerRegistration.cpp')).toContain('TEXT("arrange_nodes")');
  });
});

// set_variable_metadata (ExposeOnSpawn on BP_FloatText.Tint) saved the Blueprint but replied without assetPath,
// so the receipt listed no change; set_metadata had the same gap.
describe('Blueprint metadata writes name the Blueprint they changed', () => {
  it('both metadata handlers add the asset verification fields', () => {
    expect(read('Domains', 'Blueprint', 'Variables', 'McpAutomationBridge_BlueprintHandlersVariableMetadata.cpp'))
      .toContain('McpHandlerUtils::AddVerification(Resp, Blueprint);');
    expect(read('Domains', 'Blueprint', 'Metadata', 'McpAutomationBridge_BlueprintHandlersSetMetadata.cpp'))
      .toContain('McpHandlerUtils::AddVerification(Resp, BP);');
  });
});

// Seeing where a graph's nodes sit took one inspect_graph info=node read per node.
describe('graph details list each node position', () => {
  it('get_graph_details adds x and y to every node row', () => {
    const queries = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersQueries.cpp');
    expect(queries).toContain('NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);');
    expect(queries).toContain('NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);');
  });
});

// A modify_component operation with `rotation` given on it, not inside `transform` (the contract also
// promises "those three directly on the operation"), applied its properties, dropped the rotation and
// answered success: CameraBoom kept its yaw while TargetArmLength changed.
describe('an edit_scs operation takes location, rotation and scale directly on it, as it takes them in transform', () => {
  const scs = (file: string): string => read('Domains', 'Blueprint', 'Components', file);
  const transform = (): string => scs('McpAutomationBridge_BlueprintHandlersScsTransform.h');
  const ops = (): string => scs('McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp');

  it('folds the three keys into transform, a part already there winning, before either op handler reads it', () => {
    const fold = sliceBetween(transform(), 'inline void FoldDirect(', 'inline TArray<FString> Apply(');
    for (const key of ['location', 'rotation', 'scale']) expect(fold, key).toContain(`TEXT("${key}")`);
    expect(fold).toMatch(/Part\.IsValid\(\) && !Part->IsNull\(\) && !Transform->HasField\(Key\)/u);
    expect(fold).toContain('Op->SetObjectField(TEXT("transform"), Transform);');

    const entry = ops().slice(ops().indexOf('void ApplyModifyScsComponentOperation('));
    const folded = entry.indexOf('McpScsTransform::FoldDirect(Op);');
    expect(folded).toBeGreaterThan(-1);
    expect(folded).toBeLessThan(entry.indexOf('ApplyModifyScsModifyComponent('));
    expect(folded).toBeLessThan(entry.indexOf('ApplyModifyScsAddComponent('));
  });

  it('modify_component reads its placement through one Apply, and answers what did not land with the properties', () => {
    const modify = sliceBetween(ops(), 'void ApplyModifyScsModifyComponent(', 'void ApplyModifyScsAddComponent(');

    expect(modify).not.toContain('TransformObj');
    expect(modify).toContain('McpScsTransform::Apply(Template, Op, Defaults, bAnySuccess)');
    expect(modify).toContain('Rejected.Append(McpScsPropertyBag::Apply(Template, PropertiesObj, Defaults, bAnySuccess));');
    expect(modify).toContain('OpSummary->SetArrayField(TEXT("rejectedProperties"), Values);');
    expect(scs('McpAutomationBridge_BlueprintHandlersModifyScsFinalize.cpp')).toMatch(/did not apply %s/u);
  });

  it('names a transform that is not an object, a component that has none and a part it cannot read; writes nothing when no part was readable', () => {
    const apply = transform().slice(transform().indexOf('inline TArray<FString> Apply('));
    const write = apply.indexOf('Scene->SetRelativeLocation(Location);');

    expect(apply).toMatch(/if \(!Value->TryGetObject\(Transform\) \|\| !Transform\)\s*Rejected\.Add\(/u);
    expect(apply).toMatch(/else if \(!Scene\)\s*Rejected\.Add\(/u);
    expect(apply).toMatch(/\(Field->Type == EJson::Object && Field->AsObject\(\)->Values\.Num\(\) > 0\) \|\| \(Field->TryGetArray\(Triple\) && Triple->Num\(\) >= 3\)\)\s*\+\+Readable;\s*else\s*Rejected\.Add\(/u);
    expect(apply.indexOf('if (Readable == 0)')).toBeGreaterThan(-1);
    expect(apply.indexOf('if (Readable == 0)')).toBeLessThan(write);
    expect(apply).toMatch(/Scene->SetRelativeRotation\(Rotation\);\s*Scene->SetRelativeScale3D\(Scale\);\s*bAnyApplied = true;/u);
  });

  it('a component added through the fallback route is placed and configured too', () => {
    const fallback = sliceBetween(ops(), 'LocalSCS->CreateNode(ComponentClass', 'Failed to create SCS node');

    expect(fallback).toMatch(/if \(ScsOpHasEdits\(Op\)\) \{\s*ApplyModifyScsModifyComponent\(LocalBP, LocalSCS, Op, OpSummary\);/u);
  });

  it('the record promises the direct form and says an unreadable part is named', () => {
    const description = paramDescription('blueprint.edit_scs', 'operations');

    expect(description).toContain('(or those three directly on the operation)');
    expect(description).toMatch(/named in warnings, not skipped/u);
  });
});

// An add_component operation given only meshPath/materialPath answered success with an empty
// component: the add handed the node on to the modify path, the one that applies a mesh, only when it
// named a transform or properties.
describe('an edit_scs add_component operation applies the mesh and material it names, and names one that did not land', () => {
  const scs = (file: string): string => read('Domains', 'Blueprint', 'Components', file);
  const fields = (): string => scs('McpAutomationBridge_BlueprintHandlersScsOpFields.h');
  const assets = (): string => scs('McpAutomationBridge_BlueprintHandlersScsTemplateAssets.h');
  const ops = (): string => scs('McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp');

  it('one predicate decides whether an add has anything to hand on: a placement, properties, a mesh or a material, null being nothing', () => {
    const predicate = fields().slice(fields().indexOf('inline bool ScsOpHasEdits('));

    expect(predicate).toMatch(/for \(const TCHAR \*Key : \{TEXT\("transform"\), TEXT\("properties"\)\}\)/u);
    expect(predicate).toMatch(/Value\.IsValid\(\) && !Value->IsNull\(\)\)\s*return true;/u);
    expect(predicate).toContain('return !ScsOpMeshPath(Op).IsEmpty() || !ScsOpMaterialPath(Op).IsEmpty();');
    expect(fields()).toMatch(/McpGetFirstStringField\(Op, \{TEXT\("meshPath"\), TEXT\("mesh_path"\), TEXT\("staticMesh"\)\}\)/u);
    expect(fields()).toMatch(/McpGetFirstStringField\(Op, \{TEXT\("materialPath"\), TEXT\("material_path"\)\}\)/u);
  });

  it('every route an add takes to the modify path asks it, and none keeps the old transform-or-properties test', () => {
    const add = sliceBetween(ops(), 'void ApplyModifyScsAddComponent(', 'void ApplyModifyScsComponentOperation(');

    expect(add.match(/if \(ScsOpHasEdits\(Op\)\) \{/gu)).toHaveLength(3);
    expect(add).not.toMatch(/Op->HasField\(TEXT\("transform"\)\)/u);
    expect(add).not.toMatch(/Op->HasField\(TEXT\("properties"\)\)/u);
  });

  it('the applier reads the mesh and material through the shared spellings, and returns what did not land as "part: reason"', () => {
    const apply = assets().slice(assets().indexOf('inline TArray<FString> ApplyScsTemplateAssets('));

    expect(apply).toContain('const FString MeshPath = ScsOpMeshPath(Op);');
    expect(apply).toContain('const FString MaterialPath = ScsOpMaterialPath(Op);');
    expect(apply).not.toContain('TryGetStringField');
    expect(apply.match(/Rejected\.Add\(FString::Printf\(TEXT\("meshPath: /gu)).toHaveLength(3);
    expect(apply.match(/Rejected\.Add\(FString::Printf\(TEXT\("materialPath: /gu)).toHaveLength(2);
    expect(apply).toContain('return Rejected;');
  });

  it('the modify path puts those misses in the same rejectedProperties as the transform and the properties, so the batch warnings name them', () => {
    const modify = sliceBetween(ops(), 'void ApplyModifyScsModifyComponent(', 'void ApplyModifyScsAddComponent(');
    const applied = modify.indexOf('Rejected.Append(ApplyScsTemplateAssets(Template, Op, bAssetsApplied));');

    expect(applied).toBeGreaterThan(-1);
    expect(applied).toBeLessThan(modify.indexOf('OpSummary->SetArrayField(TEXT("rejectedProperties"), Values);'));
    expect(modify).toContain('bAnySuccess = bAssetsApplied || bAnySuccess;');
  });

  it('the record says a mesh or material that does not load is named in warnings', () => {
    expect(paramDescription('blueprint.edit_scs', 'operations')).toMatch(/as is a meshPath or materialPath that does not load or that its component cannot take/u);
  });
});

// A payload naming its class as componentType was promoted to an add (the promotion counted
// componentType), then answered "Component class not found": the add operation read only componentClass.
describe('an edit_scs add takes its class from componentType as well as componentClass, wherever one decides or reads it', () => {
  const scs = (file: string): string => read('Domains', 'Blueprint', 'Components', file);
  const fields = (): string => scs('McpAutomationBridge_BlueprintHandlersScsOpFields.h');
  const state = (): string => scs('McpAutomationBridge_BlueprintHandlersModifyScsState.cpp');
  const single = (): string => scs('McpAutomationBridge_BlueprintHandlersScsAddComponent.cpp');
  const batchAdd = (): string => sliceBetween(scs('McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp'), 'void ApplyModifyScsAddComponent(', 'void ApplyModifyScsComponentOperation(');

  it('one reader takes componentClass, component_class and componentType, in that order', () => {
    expect(fields()).toMatch(/inline FString ScsOpComponentClass\(const TSharedPtr<FJsonObject> &Op\)\s*\{\s*return McpGetFirstStringField\(Op, \{TEXT\("componentClass"\), TEXT\("component_class"\), TEXT\("componentType"\)\}\);/u);
  });

  it('the promotion, the batch add and the single add_scs_component route all read the class through it', () => {
    expect(state()).toContain('const bool bIsAdd = !ScsOpComponentClass(Op).IsEmpty();');
    expect(batchAdd()).toContain('const FString ComponentClassPath = ScsOpComponentClass(Op);');
    expect(single()).toContain('const FString ComponentClass = ScsOpComponentClass(Payload);');
  });

  it('none of them keeps a spelling list of its own for the class', () => {
    for (const [name, source] of [['promotion', state()], ['batch add', batchAdd()], ['single add', single()]] as const) {
      expect(source, name).not.toContain('TEXT("componentType")');
      expect(source, name).not.toMatch(/TEXT\("componentClass"\)/u);
      expect(source, name).not.toMatch(/TEXT\("component_class"\)/u);
    }
  });

  it('a class that does not resolve is named in the warning, and an add with none says what it needs', () => {
    expect(batchAdd()).toMatch(/OpSummary->SetStringField\(TEXT\("warning"\), ComponentClassPath\.IsEmpty\(\)\s*\?\s*FString\(TEXT\("[^"]*componentClass[^"]*componentType[^"]*"\)\)\s*:\s*FString::Printf\([^;]*\*ComponentClassPath\)\);/u);
  });

  it('the record lists componentType beside componentClass for an add', () => {
    expect(paramDescription('blueprint.edit_scs', 'operations')).toContain('componentClass (or componentType)');
  });
});

// A top-level componentType on edit=modify worked (the promotion of a single-component payload reads it) but the
// record declared it only for the add variants, so the receipt warned that the call "did not use it".
describe('the edit_scs record names every variant that reads a class or attach parameter, so the unread warning is true', () => {
  const scs = (file: string): string => read('Domains', 'Blueprint', 'Components', file);
  const record = () => {
    const found = capabilityIndex().byId.get('blueprint.edit_scs');
    if (found === undefined) throw new Error('blueprint.edit_scs is not in the catalogue');
    return found;
  };
  const owners = (name: string): readonly string[] | undefined => record().routing.dispatchBy?.declaredBy?.[name];
  const unread = (sent: readonly string[], edit: string): readonly string[] => unreadVariantParams(record(), sent, { edit });

  it('componentClass, componentType and attachTo are declared by both adds and by modify, whose top-level payload can be an add', () => {
    for (const name of ['componentClass', 'componentType', 'attachTo']) {
      expect(owners(name), name).toEqual(['add_scs_component', 'add_component', 'modify']);
    }
    expect(owners('parentComponent')).toEqual(['add_scs_component', 'add_component']);
  });

  it('a call that used them draws no warning, whichever of those variants ran', () => {
    for (const edit of ['add_scs_component', 'add_component', 'modify']) {
      expect(unread(['componentClass', 'componentType', 'attachTo'], edit), edit).toEqual([]);
    }
    for (const edit of ['add_scs_component', 'add_component']) {
      expect(unread(['parentComponent'], edit), edit).toEqual([]);
    }
  });

  it('a variant that reads none of them still gets the warning', () => {
    for (const edit of ['reparent', 'set_property', 'set_transform']) {
      expect(unread(['componentType'], edit), edit).toEqual([
        expect.stringMatching(/^componentType is read only when edit is add_scs_component or add_component or modify; this edit=\w+ call did not use it\.$/u),
      ]);
    }
    expect(unread(['parentComponent'], 'modify')).toHaveLength(1);
  });

  it('a call that leaves edit out and sends operations beside a class runs the batch variant; a plain add stays on the default', () => {
    expect(inferSelector(record(), { blueprintPath: '/Game/BP_X', componentType: 'SceneComponent', operations: [] }).edit).toBe('modify');
    expect(inferSelector(record(), { blueprintPath: '/Game/BP_X', componentName: 'C', componentType: 'SceneComponent' })).not.toHaveProperty('edit');
  });

  it('the handlers read what the record declares for those variants', () => {
    const single = scs('McpAutomationBridge_BlueprintHandlersScsAddComponent.cpp');
    const batchAdd = sliceBetween(scs('McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp'), 'void ApplyModifyScsAddComponent(', 'void ApplyModifyScsComponentOperation(');

    expect(single).toContain('const FString ComponentClass = ScsOpComponentClass(Payload);');
    expect(single).toContain('ScsFirstOf(Payload, TEXT("parent_component"), TEXT("parentComponent"))');
    expect(single).toContain('ParentName = ScsFieldOrEmpty(Payload, TEXT("attachTo"));');
    expect(scs('McpAutomationBridge_BlueprintHandlersModifyScsState.cpp')).toContain('const bool bIsAdd = !ScsOpComponentClass(Op).IsEmpty();');
    expect(batchAdd).toContain('Op->TryGetStringField(TEXT("attachTo"), AttachToName);');
  });
});

describe('graph edits that used to answer success while doing nothing useful', () => {
  // A Set node's value pin carries the variable's name but is its input; a batch that named it as a
  // link's source failed, though the node's one output of that type (Output_Get) was meant.
  it('connect_pins takes a Set node\'s variable-named source pin as its one same-type output', () => {
    const connect = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp');
    expect(connect).toMatch(/FromPin->Direction == EGPD_Input && ToPin->Direction == EGPD_Input/u);
    expect(connect).toMatch(/Pin->Direction == EGPD_Output && Pin->PinType == FromPin->PinType/u);
    expect(connect).toContain('if (SameTypeOutputs.Num() == 1) FromPin = SameTypeOutputs[0];');
    expect(connect.indexOf('SameTypeOutputs')).toBeLessThan(connect.indexOf('TryCreateConnection'));
  });

  // "-1" (no unary minus) left a math expression node with no pins under a success reply.
  it('a math expression that does not parse is refused and the node keeps its old expression', () => {
    const mutations = read('Domains', 'BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp');
    expect(mutations).toContain('TEXT("K2Node_MathExpression")');
    expect(mutations).toMatch(/McpTrySetNodeAssetPropertyForMcp\(TargetNode, PropertyName, OldExpression\);/u);
    expect(mutations).toContain('TEXT("EXPRESSION_INVALID")');
    // "x && !y" failed as a bare "does not parse": the refusal names unary ! and && on a number input.
    expect(mutations).toContain('DescribeProblems(Context.Blueprint, Value)');
  });

  // create_framework_class documents short class names, but "GameModeBase" answered NOT_FOUND.
  it('a framework class parent given by short name resolves like every other class parameter', () => {
    const utilities = read('Domains', 'GameFramework', 'McpAutomationBridge_GameFrameworkHandlersUtilities.cpp');
    expect(utilities).toContain('if (!ClassPath.Contains(TEXT("/"))) return ResolveClassByName(ClassPath);');
  });
});
