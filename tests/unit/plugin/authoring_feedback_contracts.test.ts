// Source contracts for authoring calls that used to succeed silently or fail
// with a message that pointed the caller the wrong way (found building the
// Dario Rider finale over MCP, 2026-09-24).
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const COMPONENTS = 'Domains/Blueprint/Components';

describe('edit_scs properties bag', () => {
  const bag = () => code(readCpp(COMPONENTS, 'McpAutomationBridge_BlueprintHandlersScsPropertyBag.h'));
  const ops = () => code(readCpp(COMPONENTS, 'McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp'));
  const finalize = () => code(readCpp(COMPONENTS, 'McpAutomationBridge_BlueprintHandlersModifyScsFinalize.cpp'));

  it('returns every key that did not land instead of skipping it', () => {
    const s = bag();
    expect(s).toMatch(/Rejected\.Add\(FString::Printf\(TEXT\("%s: %s"\), \*Name,\s*Error\.IsEmpty\(\)/);
    expect(s).toMatch(/Rejected\.Add\(FString::Printf\(TEXT\("%s: %s"\), \*Name, Failure\.IsEmpty\(\)/);
    expect(ops()).toMatch(/SetArrayField\(TEXT\("rejectedProperties"\)/);
    expect(finalize()).toMatch(/TryGetArrayField\(TEXT\("rejectedProperties"\)/);
  });

  it('routes collision names through the component setters', () => {
    const s = bag();
    expect(s).toMatch(/SetCollisionProfileName\(FName\(\*Text\)\)/);
    expect(s).toMatch(/SetCollisionEnabled\(static_cast<ECollisionEnabled::Type>\(Value\)\)/);
    expect(s).toMatch(/TEXT\("BodyInstance\."\)/);
  });

  it('writes placed instances the way edit_component does, only placed ones, and counts only writes that held', () => {
    const s = code(readCpp(COMPONENTS, 'McpAutomationBridge_BlueprintHandlersScsPropagate.h'));
    const propagate = s.slice(s.indexOf('int32 Propagate('), s.indexOf('inline TArray<TPair<'));
    expect(propagate).toMatch(/World->WorldType != EWorldType::Editor/);
    // A text import plus a full re-register reported every bool (bVisible, CastShadow)
    // as updated while the placed component kept its old value.
    expect(propagate).toMatch(/Component->Modify\(\);/);
    expect(propagate).toMatch(/Prop->CopyCompleteValue_InContainer\(Container, TemplateContainer\)/);
    expect(propagate).toMatch(/McpRefreshComponentAfterEdit\(Live\)/);
    expect(propagate).not.toMatch(/FComponentReregisterContext/);
    // Read back: a value that did not hold is never counted as updated.
    expect(propagate).toMatch(/ExportNested\(Component, Entry\.Key, After\) && After == Entry\.Value/);
    expect(propagate).toMatch(/OutFailed->Add\(/);
    expect(ops()).toMatch(/McpScsPropagate::PropagateAndReport\(Defaults, OpSummary\)/);
    expect(s).toMatch(/SetArrayField\(TEXT\("updatedInstances"\), ToJsonStrings\(UpdatedPaths\)\)/);
  });

  it('pushes template defaults to placed instances again after the final compile and names any that did not take', () => {
    expect(ops()).toMatch(/McpScsPropagate::Pending\(\)\.Emplace\(Template, Defaults\)/);
    const s = finalize();
    const compile = s.indexOf('McpCompileBlueprintWithDiagnostics(');
    const repropagate = s.indexOf('McpScsPropagate::RepropagatePending(&RepropagateMissed)');
    expect(compile).toBeGreaterThan(-1);
    expect(repropagate).toBeGreaterThan(compile);
    expect(s).toMatch(/placed instance kept its old value: %s/);
    expect(code(readCpp(COMPONENTS, 'McpAutomationBridge_BlueprintHandlersModifyScs.cpp')))
      .toMatch(/McpScsPropagate::Pending\(\)\.Reset\(\);\s*for \(int32 Index = 0;/);
  });
});

describe('writes to a live component refresh what is drawn', () => {
  it('shares one refresh helper between edit_component and inspect.set_property', () => {
    const helper = code(readCpp('Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h'));
    expect(helper).toMatch(/McpRefreshComponentAfterEdit\(UActorComponent \*Component\)/);
    expect(helper).toMatch(/FComponentReregisterContext ReregisterContext\(Component\)/);
    expect(helper).toMatch(/MarkRenderStateDirty\(\)/);
    expect(code(readCpp('Domains/Property/McpAutomationBridge_PropertyHandlersObjectSet.cpp')))
      .toMatch(/PostEditChange\(\);\s*McpRefreshComponentAfterEdit\(Cast<UActorComponent>\(RootObject\)\);/);
    expect(code(readCpp('Domains/ControlActor/McpAutomationBridge_ControlActorComponentProperties.cpp')))
      .toMatch(/McpRefreshComponentAfterEdit\(TargetComponent\);/);
  });
});

describe('graph pin literals', () => {
  const GRAPH = 'Domains/BlueprintGraph';

  it('feeds a read-only pin through a MakeLiteral node instead of refusing it', () => {
    // K2_SetText takes `const FText&`: its Value pin has no literal box, and the
    // refusal stopped a Dario Rider build_graph at step 37 of 80 (2026-09-26).
    const s = code(readCpp(GRAPH, 'PinMutations/McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp'));
    expect(s).toMatch(/if \(Pin->bDefaultValueIsIgnored\)\s*\{\s*return FeedReadOnlyPinLiteral\(Context, \*TargetNode, \*Pin, Value\);/);
    expect(s.indexOf('FeedReadOnlyPinLiteral')).toBeLessThan(s.indexOf('FScopedTransaction Transaction('));
    const literal = code(readCpp(GRAPH, 'PinMutations/McpAutomationBridge_BlueprintGraphPinLiteralNode.cpp'));
    // Verified live: MakeLiteralText is declared on KismetSystemLibrary, not KismetTextLibrary.
    expect(literal).toMatch(/UKismetSystemLibrary::StaticClass\(\)->FindFunctionByName\(FunctionName\)/);
    for (const [category, fn] of [['PC_Text', 'MakeLiteralText'], ['PC_String', 'MakeLiteralString'], ['PC_Name', 'MakeLiteralName'], ['PC_Real', 'MakeLiteralDouble']]) {
      expect(literal).toContain(`{UEdGraphSchema_K2::${category}, TEXT("${fn}")}`);
    }
    // A second set updates the literal already feeding the pin instead of stacking another.
    expect(literal).toMatch(/UEdGraphPin\* ValuePin = FeedingLiteralValuePin\(Pin\);/);
    // Structs, enum bytes and arrays have no MakeLiteral and keep the refusal.
    expect(literal).toMatch(/PIN_REQUIRES_CONNECTION/);
    expect(literal).toMatch(/Type\.IsContainer\(\) \|\| \(Type\.PinCategory == UEdGraphSchema_K2::PC_Byte && Type\.PinSubCategoryObject\.IsValid\(\)\)/);
  });

  it('removes a build_graph step node whose pin defaults failed', () => {
    const s = code(readCpp(GRAPH, 'McpAutomationBridge_BlueprintGraphHandlersBatchSteps.cpp'));
    const rollback = s.indexOf('RemoveNodeWithLiterals(Context.Blueprint, Context.FindNode(Guid));');
    expect(rollback).toBeGreaterThan(s.indexOf('ApplyPinDefaults(Context, Payload, Step, Guid, StepId, Entry)'));
    // Nor does its reply echo the pin defaults that went with the node.
    expect(s.indexOf('Entry->RemoveField(TEXT("pinDefaults"));')).toBeGreaterThan(rollback);
    // The alias is published only once the step fully succeeded.
    expect(s.indexOf('State.Aliases.Add(Alias, Guid);')).toBeGreaterThan(rollback);
  });

  it('saves again after the response compile, which dirties the package', () => {
    // A lone connect_pins left BP_MarioGM dirty: saved, then compiled (2026-09-26).
    const s = code(readCpp(GRAPH, 'Context/McpAutomationBridge_BlueprintGraphHandlersContextShared.cpp'));
    const compile = s.indexOf('McpCompileBlueprintWithDiagnostics(Blueprint, Result, FirstError, 6)');
    expect(s.indexOf('Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Blueprint));')).toBeGreaterThan(compile);
  });

  it('never compiles on a read', () => {
    // A failed batch left BP_WaitlistDoor dirty; the next inspect_graph compiled
    // it and the compile reset the editor's undo history.
    const s = code(readCpp(GRAPH, 'McpAutomationBridge_BlueprintGraphHandlers.cpp'));
    const defer = s.indexOf('Context.bDeferCompile = true;');
    expect(defer).toBeGreaterThan(s.indexOf('HandleNodeMutationAction(Context)'));
    expect(defer).toBeLessThan(s.indexOf('HandleNodeQueryAction(Context)'));
    expect(defer).toBeLessThan(s.indexOf('HandleNodeDetailAction(Context)'));
  });

  it('names the connect endpoint that was not found', () => {
    const s = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp'));
    expect(s).toMatch(/!FromNode \? TEXT\("source"\) : TEXT\("target"\)/);
    expect(s).not.toMatch(/Could not find source or target node\./);
  });

  it('takes GetVariable/SetVariable on create_node and in the build_graph pre-check', () => {
    // edit_graph's contract names GetVariable; create_node answered NODE_TYPE_NOT_FOUND (2026-09-26).
    const nodes = code(readCpp(GRAPH, 'McpAutomationBridge_BlueprintGraphHandlersVariableNodes.cpp'));
    expect(nodes).toContain('Type.Equals(TEXT("GetVariable"), ESearchCase::IgnoreCase)');
    expect(nodes).toContain('Type.Equals(TEXT("SetVariable"), ESearchCase::IgnoreCase)');
    expect(nodes).toMatch(/if \(!ParseVariableNodeType\(NodeType, bIsSet\)\)/);
    const batch = code(readCpp(GRAPH, 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp'));
    expect(batch).toMatch(/ParseVariableNodeType\(NodeType, bSetNode\) && MemberClass\.IsEmpty\(\)/);
  });

  it('names the graphs, or the event graph an event lives in, when a graph is not found', () => {
    // graphName "FoundSecret" (a custom event in EventGraph) answered a bare "Could not find graph".
    const s = code(readCpp(GRAPH, 'Context/McpAutomationBridge_BlueprintGraphHandlersContextEditor.cpp'));
    expect(s).toContain('Context.SendError(DescribeMissingGraph(Context.Blueprint, GraphName), TEXT("GRAPH_NOT_FOUND"));');
    expect(s).toMatch(/Event->GetFunctionName\(\)\.ToString\(\)\.Equals\(GraphName, ESearchCase::IgnoreCase\)/);
    expect(s).toContain("is an event inside graph '%s', not a graph");
    expect(s).toContain('Its graphs: %s.');
  });

  it('explains an unflagged Widget Blueprint widget instead of a bare not-found', () => {
    const s = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersVariableNodes.cpp'));
    expect(s).toMatch(/FindObject<UObject>\(Context\.Blueprint, TEXT\("WidgetTree"\)\)/);
    expect(s).toMatch(/is not marked as a variable/);
  });
});

describe('asset listing filter', () => {
  it('narrows a listing by asset name when filter is a string', () => {
    const s = code(readCpp('Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowListing.cpp'));
    expect(s).toMatch(/Payload->TryGetStringField\(TEXT\("filter"\), NameFilter\)/);
    expect(s).toMatch(/Asset\.AssetName\.ToString\(\)\.Contains\(NameFilter, ESearchCase::IgnoreCase\)/);
  });
});

describe('create_node without a position', () => {
  // It landed at (0,0), hit the overlap guard, and the caller had to retry at the suggestion.
  it('steps right past the overlapped nodes instead of refusing, only when no position was given', () => {
    const header = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h'));
    expect(header).toMatch(/for \(int32 Step = 0; bOverlaps && bAutoPlace && Step < 8; \+\+Step\)/);
    expect(header).toMatch(/Occupant\.X \+ Occupant\.Width \+ McpGraphLayout::NodeSuggestGap/);
    expect(header).toMatch(/const bool bAutoPlace = Payload\.IsValid\(\) &&\s*!Payload->HasField\(TEXT\("x"\)\)/);
  });
});

describe('material instance parameter names', () => {
  // A name the parent does not publish was stored as a dead override and reported as set.
  it.each([
    ['SetScalarParameterValue', 'GetAllScalarParameterInfo', 'SetScalarParameterValueEditorOnly'],
    ['SetVectorParameterValue', 'GetAllVectorParameterInfo', 'SetVectorParameterValueEditorOnly'],
    ['SetTextureParameterValue', 'GetAllTextureParameterInfo', 'SetTextureParameterValueEditorOnly'],
  ])('%s checks the name against the instance before writing', (file, lookup, write) => {
    const s = code(readCpp(`Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringHandlers${file}.cpp`));
    const check = s.indexOf(`Instance->${lookup}(`);
    expect(check).toBeGreaterThan(-1);
    expect(s.indexOf(`Instance->${write}(`)).toBeGreaterThan(check);
    expect(s).toContain('TEXT("PARAMETER_NOT_FOUND")');
  });
});

describe('material connect_nodes output names', () => {
  it('matches any expression output name, maps RGB to the default output, and lists outputs on a miss', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Connections/McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp'));
    expect(s).toMatch(/SourceExpr->GetOutputs\(\)/);
    expect(s).toMatch(/SourcePin\.Equals\(TEXT\("RGB"\), ESearchCase::IgnoreCase\) \|\|/);
    expect(s).not.toMatch(/source node has no named outputs/);
  });

  it('wires through the engine connect, so the output channel mask rides on the wire', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Connections/McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp'));
    expect(s).toMatch(/SourceExpr->ConnectExpression\(&Input, SourceOutputIndex\)/);
    // Every write goes through Wire; a bare Expression/OutputIndex write drops the mask.
    expect(s).not.toMatch(/->Expression = SourceExpr|\.Expression = SourceExpr/);
    expect(s).toMatch(/GetOutputs\(\)\.IsValidIndex\(SourceOutputIndex\)/);
  });

  it('reads channel letters on a single-output node as a mask of its default output', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Connections/McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp'));
    expect(s).toMatch(/for \(const TCHAR\* Set : \{TEXT\("RGBA"\), TEXT\("XYZW"\)\}\)/);
    expect(s).toMatch(/ParseChannelMask\(SourcePin, Channels\)\) \{\s*SourceOutputIndex = 0;\s*bChannelMask = true;/);
    // An unnamed output is listed as (default), not as "None".
    expect(s).toMatch(/OutputName\.IsNone\(\) \? FString\(\)/);
  });

  it('matches a target input by the label the node draws and lists the labels on a miss', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Connections/McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp'));
    expect(s).toMatch(/FExpressionInput \*Input = TargetExpr->GetInput\(InputIndex\)/);
    // A function call labels its inputs "Name (Type)"; the bare name must match too.
    expect(s).toMatch(/Label\.Split\(TEXT\(" \("\), &Plain, nullptr\)/);
    expect(s).toMatch(/not found on %s\. Its inputs: %s\./);
  });
});

describe('material compile results', () => {
  it('compile_material reports the translator errors instead of always answering compiled', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Properties/McpAutomationBridge_MaterialAuthoringHandlersCompileMaterial.cpp'));
    expect(s).toMatch(/MCP_GET_MATERIAL_RESOURCE\(Material\)/);
    expect(s).toMatch(/CompileErrors = Resource->GetCompileErrors\(\);/);
    expect(s).toMatch(/SetBoolField\(TEXT\("compiled"\), CompileErrors\.Num\(\) == 0\)/);
    expect(s).not.toMatch(/SetBoolField\(TEXT\("compiled"\), true\)/);
  });

  it('looks the material resource up by feature level through 5.6 and by shader platform from 5.7', () => {
    const s = readCpp('Core/Compatibility/McpVersionCompatibility.h');
    expect(s).toMatch(/ENGINE_MINOR_VERSION >= 7\)\s*#define MCP_GET_MATERIAL_RESOURCE\(Material\) \(Material\)->GetMaterialResource\(GMaxRHIShaderPlatform\)/);
    expect(s).toMatch(/#define MCP_GET_MATERIAL_RESOURCE\(Material\) \(Material\)->GetMaterialResource\(GMaxRHIFeatureLevel\)/);
  });

  it('the batch passes the compile verdict and errors through', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringGraphBatch.cpp'));
    expect(s).toMatch(/Compiled\.Result->TryGetBoolField\(TEXT\("compiled"\), bCompiles\)/);
    expect(s).toMatch(/Result->SetArrayField\(TEXT\("compileErrors"\), \*CompileErrors\)/);
  });

  it('add_component_mask sets only the channels named when any are named', () => {
    const s = code(readCpp('Domains/MaterialAuthoring/Nodes/McpAutomationBridge_MaterialAuthoringHandlersAddComponentMask.cpp'));
    expect(s).toMatch(/bool bR = !bNamed, bG = !bNamed, bB = !bNamed, bA = false;/);
  });
});

describe('parameter setters probe for an instance quietly', () => {
  // A base material answered correctly but left "Failed to find object
  // 'MaterialInstanceConstant ...'" in the receipt's warnings.
  it.each(['SetScalarParameterValue', 'SetVectorParameterValue', 'SetTextureParameterValue', 'SetStaticSwitchParameterValue'])(
    '%s loads the instance with LOAD_NoWarn', (file) => {
      const s = code(readCpp(`Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringHandlers${file}.cpp`));
      expect(s).toMatch(/LoadObject<UMaterialInstanceConstant>\(nullptr, \*AssetPath, nullptr, LOAD_NoWarn \| LOAD_Quiet\)/);
    });
});

describe('inspect_object componentName', () => {
  // Declared but never read: a light's Intensity lives on its light component,
  // so the actor read listed every requested name under missingProperties.
  it('reads the named component and lists the actor components on a miss', () => {
    const s = code(readCpp('Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectObject.cpp'));
    expect(s).toMatch(/McpHandlerUtils::FindActorComponentByName\(Owner, ComponentName\)/);
    expect(s).toMatch(/McpAppendPropertyDump\(DumpTarget, PropertyNames, Resp\)/);
    expect(s).toMatch(/Its components: %s\./);
  });
});

describe('sun and directional light angles', () => {
  const ENV = 'Domains/Environment';
  it('pitches a light down by the elevation everywhere a sun is aimed', () => {
    expect(readCpp(ENV, 'McpAutomationBridge_EnvironmentHandlersShared.h'))
      .toMatch(/McpSunRotation\(double Elevation, double Azimuth\)\s*\{\s*return FRotator\(static_cast<float>\(-Elevation\)/);
    const sun = code(readCpp(ENV, 'Runtime/McpAutomationBridge_EnvironmentHandlersWeatherActors.cpp'));
    expect(sun).toMatch(/double Elevation = -SunActor->GetActorRotation\(\)\.Pitch;/);
    expect(sun).toMatch(/SetActorRotation\(McpSunRotation\(Elevation, Azimuth\)\)/);
    // pitch = elevation pointed a noon sun straight up.
    expect(code(readCpp(ENV, 'Runtime/McpAutomationBridge_EnvironmentHandlersTimeWater.cpp')))
      .not.toMatch(/FRotator\(static_cast<float>\(Elevation\)/);
  });

  it('turns a directional light azimuth/elevation into its rotation and names settings keys nothing declares', () => {
    const s = code(readCpp(ENV, 'Runtime/McpAutomationBridge_EnvironmentHandlersActorComponents.cpp'));
    expect(s).toMatch(/Actor->IsA<ADirectionalLight>\(\)/);
    expect(s).toMatch(/Actor->SetActorRotation\(McpSunRotation\(Elevation, Azimuth\)\)/);
    expect(s).toMatch(/!McpFindPropertyCaseInsensitive\(Actor, Key\) && !\(Component && McpFindPropertyCaseInsensitive\(Component, Key\)\)/);
    expect(s).toMatch(/%s: %s has no such property/);
  });
});

describe('FColor properties from 0-1 channels', () => {
  // A light color sent as {R: 0.6, G: 0.7, B: 1} was truncated to bytes: black.
  it('reads channels that all lie within 0-1 as normalized before the converter runs', () => {
    const imp = code(readCpp('Foundation/Reflection/McpPropertyReflectionImport.cpp'));
    const color = imp.indexOf('ScriptStruct == TBaseStructure<FColor>::Get() && Private::TryImportNormalizedColor(');
    expect(color).toBeGreaterThan(-1);
    expect(color).toBeLessThan(imp.indexOf('FJsonObjectConverter::JsonObjectToUStruct('));
    const helper = code(readCpp('Foundation/Reflection/McpPropertyReflectionText.cpp'));
    expect(helper).toMatch(/if \(Values\[Index\] < 0\.0 \|\| Values\[Index\] > 1\.0\) return false;/);
    expect(helper).toMatch(/FMath::RoundToInt\(Values\[Index\] \* 255\.0\)/);
  });

  it('the older property applier (SCS templates, CDO defaults) reads them the same way', () => {
    const s = code(readCpp('Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyObjects.h'));
    const color = s.indexOf('McpPropertyReflection::Private::TryImportNormalizedColor(');
    expect(color).toBeGreaterThan(-1);
    expect(color).toBeLessThan(s.lastIndexOf('FJsonObjectConverter::JsonObjectToUStruct('));
  });
});

describe('actor list reads named properties', () => {
  // Finding which ? blocks held what took one inspect_object call per block.
  it('reads each propertyNames entry on every listed actor into its properties object', () => {
    const s = code(readCpp('Domains/ControlActor/McpAutomationBridge_ControlActorLookup.cpp'));
    expect(s).toMatch(/Payload->TryGetArrayField\(TEXT\("propertyNames"\), PropertyNamesArray\)/);
    expect(s).toMatch(/Actor->GetClass\(\)->FindPropertyByName\(PropertyName\)/);
    expect(s).toMatch(/Entry->SetObjectField\(TEXT\("properties"\), Properties\)/);
  });
});

describe('a Blueprint graph node that is not found', () => {
  // After duplicate, every GUID copied from the source Blueprint read as a bare "Node not found.".
  it('says names still work and where to list them, from one shared helper', () => {
    const shared = code(readCpp('Domains/BlueprintGraph/Context/McpAutomationBridge_BlueprintGraphHandlersContextShared.cpp'));
    expect(shared).toMatch(/void FActionContext::SendNodeNotFound\(const FString& Id\) const/);
    expect(shared).toMatch(/a duplicated Blueprint gets new GUIDs but keeps the names/);
    for (const file of [
      'Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersDetails.cpp',
      'Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersNodeMutations.cpp',
      'Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp',
      'Domains/BlueprintGraph/PinMutations/McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp',
    ]) {
      const s = code(readCpp(file));
      expect(s, file).not.toMatch(/TEXT\("Node not found\."\)/);
      expect(s, file).toMatch(/Context\.SendNodeNotFound\(NodeId\);/);
    }
  });
});

describe('set_blueprint_variables on many actors', () => {
  // Eight billboard headlines were eight calls.
  it('runs each actors item through the single-actor path and names the ones that did not take', () => {
    const s = code(readCpp('Domains/ControlActor/McpAutomationBridge_ControlActorAdvanced.cpp'));
    expect(s).toMatch(/Payload->TryGetArrayField\(TEXT\("actors"\), Items\)/);
    expect(s).toMatch(/HandleControlActorSetBlueprintVariables\(ItemId, One, Socket\)/);
    expect(s).toMatch(/One->RemoveField\(TEXT\("actors"\)\)/);
    expect(s).toMatch(/VARIABLE_BATCH_INCOMPLETE/);
  });
});

describe('event nodes by the name the editor shows', () => {
  // "ActorBeginOverlap" was EVENT_NOT_FOUND: only BeginPlay/Tick/EndPlay had a Receive alias.
  it('tries the name as given, then the Receive-prefixed spelling, and lists the overridable events on a miss', () => {
    const s = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersFunctionEventNodes.cpp'));
    expect(s).toMatch(/Candidates\.Add\(EventName\);\s*if \(!EventName\.StartsWith\(TEXT\("Receive"\)\)\)\s*\{\s*Candidates\.Add\(TEXT\("Receive"\) \+ EventName\);/);
    expect(s).toMatch(/EventName\.RemoveFromStart\(TEXT\("Event "\)\);/);
    expect(s).toMatch(/Its overridable events: %s\./);
    expect(s).not.toMatch(/TMap<FString, FString> Aliases/);
  });
});

describe('library functions resolve when the library is named wrong', () => {
  // GetGameTimeInSeconds with memberClass GameplayStatics stopped a whole batch half-applied.
  it('takes the one library that declares it and searches String/Text by default', () => {
    // One resolver serves node creation and the build_graph pre-check.
    const nodes = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersFunctionEventNodes.cpp'));
    expect(nodes).toMatch(/UFunction\* Function = ResolveGraphCallFunction\(Context\.Blueprint, MemberName, MemberClass,/);
    const s = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlers.cpp'));
    expect(s).toMatch(/static UFunction\* FindUniqueLibraryFunction\(const FString& Name\)/);
    expect(s).toMatch(/if \(Found\)\s*\{\s*return nullptr;/);
    expect(s).toMatch(/IsChildOf\(UBlueprintFunctionLibrary::StaticClass\(\)\)\)\s*\{\s*Function = FindUniqueLibraryFunction\(MemberName\);/);
    expect(s).toMatch(/UKismetStringLibrary::StaticClass\(\),\s*UKismetTextLibrary::StaticClass\(\)/);
  });
});

describe('"execute" reaches a latent or macro node\'s own exec input', () => {
  // MoveComponentTo takes Move/Stop/Return, so a wire into "execute" was PIN_NOT_FOUND.
  it('falls back to the first exec input pin', () => {
    const s = code(readCpp('Domains/BlueprintGraph/Context/McpAutomationBridge_BlueprintGraphHandlersContextEditor.cpp'));
    expect(s).toMatch(/CleanPinName\.Equals\(TEXT\("execute"\), ESearchCase::IgnoreCase\)/);
    expect(s).toMatch(/Pin->Direction == EGPD_Input &&\s*Pin->PinType\.PinCategory == FName\(TEXT\("exec"\)\)/);
  });
});

describe('get_blueprint reads inherited properties off the CDO', () => {
  // AutoPossessAI read as PROPERTY_NOT_FOUND: only the Blueprint's own variables were searched.
  it('falls back to the generated class default object', () => {
    const s = code(readCpp('Domains/Blueprint/Queries/McpAutomationBridge_BlueprintHandlersGet.cpp'));
    expect(s).toMatch(/Generated->FindPropertyByName\(\*PropertyName\)/);
    expect(s).toMatch(/GetPropertyValueAsString\(\s*Generated->GetDefaultObject\(\), CdoProperty\)/);
  });
});

describe('blueprint path resolution during PIE', () => {
  it('asks the asset registry, never UEditorAssetLibrary, which refuses while PIE runs', () => {
    // Every manage_blueprint call in PIE logged "The Editor is currently in a play
    // mode." and blueprint_exists answered false for a real Blueprint (2026-09-26).
    const BP = 'Foundation/BridgeHelpers/Blueprints';
    const QUERIES = 'Domains/Blueprint/Queries';
    const paths = code(readCpp(BP, 'McpAutomationBridgeHelpersBlueprintPaths.h'));
    expect(paths).toMatch(/GetAssetsByPackageName\(FName\(\*PackagePath\), Found\)/);
    expect(paths).toContain('if (McpAssetExists(CheckPath)) {');
    for (const [dir, file] of [[BP, 'McpAutomationBridgeHelpersBlueprintPaths.h'], [BP, 'McpAutomationBridgeHelpersBlueprintAssetLoad.h'],
      [QUERIES, 'McpAutomationBridge_BlueprintHandlersEnsureProbe.cpp'], [QUERIES, 'McpAutomationBridge_BlueprintHandlersProbeCreateExists.cpp']]) {
      expect(code(readCpp(dir, file)), file).not.toContain('DoesAssetExist(');
    }
    // probe_handle takes the class from that registry entry.
    expect(code(readCpp(QUERIES, 'McpAutomationBridge_BlueprintHandlersEnsureProbe.cpp'))).toContain('McpAssetExists(CheckPath, &AssetData)');
  });
});

describe('get_blueprint bool defaults', () => {
  it('reports a bool default as a JSON boolean on both the compiled and the authored path', () => {
    // "bSecret": "False" came back as [REDACTED]: a string under a secret-named key (2026-09-26).
    const s = code(readCpp('Domains/Blueprint/Variables/McpAutomationBridge_BlueprintHandlersVariableIntrospection.cpp'));
    expect(s).toContain('Defaults->SetBoolField(VariableName, BoolProperty->GetPropertyValue(PropertyAddress));');
    expect(s).toContain('Defaults->SetBoolField(VariableName, Declared.DefaultValue.ToBool());');
  });
});

describe('read_log text filter', () => {
  it('treats | as alternatives in both the editor buffer and a log file tail', () => {
    // "LoadMap|Bringing World" was read as one literal and answered "Read 0 of 0".
    const s = code(readCpp('Domains/Log/McpAutomationBridge_LogHistory.cpp'));
    expect(s).toMatch(/Contains\.ParseIntoArray\(Out, TEXT\("\|"\), true\);/);
    expect(s.match(/const TArray<FString> Alternatives = SplitAlternatives\(Contains\);/g)).toHaveLength(2);
    expect(s).toContain('!ContainsAny(Line.Message, Alternatives) &&');
    expect(s).toContain('!ContainsAny(All[Index], Alternatives)');
  });
});
