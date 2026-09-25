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
  it('refuses a read-only pin with what it needs, before opening a transaction', () => {
    const s = code(readCpp('Domains/BlueprintGraph/PinMutations/McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp'));
    const refusal = s.indexOf('if (Pin->bDefaultValueIsIgnored)');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(s.indexOf('FScopedTransaction Transaction('));
    expect(s).toMatch(/PIN_REQUIRES_CONNECTION/);
    // Verified live: MakeLiteralText is declared on KismetSystemLibrary, not KismetTextLibrary.
    expect(s).toMatch(/MakeLiteralText node \(memberClass \/Script\/Engine\.KismetSystemLibrary\)/);
  });

  it('names the connect endpoint that was not found', () => {
    const s = code(readCpp('Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp'));
    expect(s).toMatch(/!FromNode \? TEXT\("source"\) : TEXT\("target"\)/);
    expect(s).not.toMatch(/Could not find source or target node\./);
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

