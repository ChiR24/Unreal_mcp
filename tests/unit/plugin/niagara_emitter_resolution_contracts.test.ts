// Which emitter (and system) a Niagara authoring edit acts on.
//
// Measured live on UE 5.8 against dev 591d3b0e: on a system with one emitter, a module, a dynamic input or a
// parameter binding sent with an emitterName the system does not have (or "DefaultEmitter") landed in the sole
// emitter and answered success; set_parameter_value without emitterName wrote the module input in every emitter
// that has it; systemPath and assetPath naming two systems edited systemPath and answered success.
// set_parameter_value needs emitterName only when the name matches in more than one place (maintainer review of
// #658): system-stage inputs and static switches, and a full Constants.<Emitter>.<Module>.<Input>, need none.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const DIR = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'NiagaraAuthoring');
const code = (file: string): string =>
  readFileSync(join(DIR, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const context = (): string => code('McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');
const values = (): string => code('McpAutomationBridge_NiagaraAuthoringHandlersParameterValues.cpp');
const owner = (): string => code('McpAutomationBridge_NiagaraAuthoringHandlersInputOwner.cpp');
const resolver = (): string => sliceBetween(context(), 'FNiagaraEmitterHandle* FindTargetEmitter(', 'FNiagaraEmitterHandle* ResolveEmitterHandle(');

describe('Niagara emitter resolution', () => {
  it('falls back to the sole emitter only for an omitted emitterName, and says so', () => {
    const resolve = resolver();
    expect(resolve).toMatch(/if \(!Handle && Context\.EmitterName\.IsEmpty\(\) && Handles\.Num\(\) == 1\)/u);
    expect(resolve).toContain('TEXT("emitterResolvedBy"), TEXT("single-emitter-fallback")');
    // A chosen name that misses must not reach the fallback by another route.
    expect(resolve.match(/Handles\[0\]/gu) ?? []).toHaveLength(1);
  });

  it('refuses a missing emitter naming the system, its emitters and that nothing changed', () => {
    const resolve = resolver();
    expect(resolve).toContain('(its emitters: [%s])');
    expect(resolve).toContain('Nothing was changed.');
    expect(resolve).toContain('System->GetPathName()');
    expect(resolve).toMatch(/return nullptr;\s*\}/u);
    expect(sliceBetween(context(), 'FNiagaraEmitterHandle* ResolveEmitterHandle(', 'bool LoadSystemAndEmitter('))
      .toMatch(/if \(!Handle\) \{ Context\.SendError\(Error, TEXT\("EMITTER_NOT_FOUND"\)\); \}/u);
  });

  it('names the system and emitter a successful edit acted on', () => {
    const resolve = resolver();
    expect(resolve).toContain('Context.EmitterName = Handle->GetName().ToString();');
    expect(resolve).toContain('Context.Result->SetStringField(TEXT("systemPath"), System->GetPathName());');
    expect(resolve).toContain('Context.Result->SetStringField(TEXT("emitterName"), Context.EmitterName);');
  });

  it('routes every emitter edit through the one resolver', () => {
    const load = sliceBetween(context(), 'bool LoadSystemAndEmitter(', 'void MarkDirtyAndVerify(');
    expect(load).toContain('Handle = System ? ResolveEmitterHandle(Context, System) : nullptr;');
    expect(load).not.toContain('FindEmitterHandle(');
  });

  it('refuses systemPath and assetPath that name different systems before any edit', () => {
    const source = context();
    const check = sliceBetween(source, 'static bool ValidateOneSystem(', 'bool ValidateCommonFields(');
    expect(check).toContain('FPackageName::ObjectPathToPackageName(Context.SystemPath).Equals(FPackageName::ObjectPathToPackageName(Context.AssetPath), ESearchCase::IgnoreCase)');
    expect(check).toContain('TEXT("CONFLICTING_TARGET")');
    expect(sliceBetween(source, 'bool ValidateCommonFields(', 'UNiagaraSystem* LoadSystemOrError(')).toContain('&& ValidateOneSystem(Context);');
  });

  it('set_parameter_value resolves a chosen emitterName, else asks where the name matches, and not for a user parameter', () => {
    const source = values();
    expect(sliceBetween(source, 'static bool NamesModuleInput(', '// One value written')).toContain('!Name.IsEmpty() && !FindUserParameter(System, Name)');
    const single = source.slice(source.indexOf('bool SetParameterValue(FActionContext& Context)'));
    const gate = '&& !(Context.EmitterName.IsEmpty() ? ResolveInputEmitter(Context, System, ParamName) : ResolveEmitterHandle(Context, System) != nullptr))';
    expect(single).toContain('if (NamesModuleInput(System, {ParamName})');
    expect(single.indexOf(gate)).toBeGreaterThan(-1);
    expect(single.indexOf(gate)).toBeLessThan(single.indexOf('WriteParameter(System, Context.EmitterName, ParamName, Context.Payload)'));
  });

  // System-stage inputs, an input of one emitter and a full Constants.<Emitter>.<Module>.<Input> need no emitterName.
  it('requires emitterName only when the name matches in more than one place', () => {
    const find = sliceBetween(owner(), 'bool FindInputEmitter(', 'bool ResolveInputEmitter(');
    expect(find).toContain('TArray<FString> Owners = FindInputOwners(System, ParamName);');
    expect(find).toMatch(/OutEmitter = Owners\.Num\(\) == 1 \? Owners\[0\] : FString\(\);\s*if \(Owners\.Num\(\) <= 1\)\s*\{\s*return true;\s*\}/u);
    expect(find).toContain('TEXT("needs emitterName: one of [%s]%s")');
    const reply = sliceBetween(owner(), 'bool ResolveInputEmitter(', '\n}');
    expect(reply).toContain('TEXT("EMITTER_NOT_FOUND")');
    expect(reply).toContain('Nothing was changed.');
    expect(reply).toContain('Context.Result->SetStringField(TEXT("emitterName"), Context.EmitterName);');
  });

  it('finds where a name matches as the writers match it: module inputs by full name or suffix, else static switches', () => {
    const source = owner();
    const owners = sliceBetween(source, 'static TArray<FString> FindInputOwners(', 'FString ListEmitterNames(');
    expect(owners).toContain('if (Name == ParamName || Name.EndsWith(TEXT(".") + ParamName))');
    expect(sliceBetween(values(), 'int32 SetModuleInputValue(', '\n}')).toContain('if (Name != ParamName && !Name.EndsWith(TEXT(".") + ParamName))');
    expect(owners).toContain('Owners.AddUnique(RapidIterationOwner(System, Name));');
    // The emitter segment of the rapid-iteration name, or none: the system stage.
    const ri = sliceBetween(source, 'static FString RapidIterationOwner(', 'static bool GraphHasStaticSwitch(');
    expect(ri).toContain('Name.StartsWith(TEXT("Constants.") + Handle.GetName().ToString() + TEXT("."))');
    expect(ri).toMatch(/return FString\(\);\s*\}\s*$/u);
    // Static switches only when no module input matched, on the system stack graph and every emitter's.
    expect(owners).toContain('if (Owners.Num() > 0 || !ParamName.Split(TEXT("."), &ModuleName, &SwitchName))');
    expect(owners).toMatch(/GraphHasStaticSwitch\(SystemSource->NodeGraph, ModuleName, SwitchName\)\)\s*\{\s*Owners\.Add\(FString\(\)\);/u);
    expect(owners).toContain('GraphHasStaticSwitch(Source->NodeGraph, ModuleName, SwitchName)');
    expect(sliceBetween(source, 'static bool GraphHasStaticSwitch(', 'static TArray<FString> FindInputOwners(')).toContain('IsStaticSwitchPin(Pin) && Pin->PinName.ToString().Equals(SwitchName, ESearchCase::IgnoreCase)');
  });

  // A list fails only the entries it cannot place, each with a short reason; its reply never says nothing changed.
  it('a parameters list resolves each entry on its own and fails only those it cannot place', () => {
    const list = sliceBetween(values(), 'static bool SetParameterValueList(', 'bool SetParameterValue(FActionContext& Context)');
    expect(list).toMatch(/if \(!Context\.EmitterName\.IsEmpty\(\) && NamesModuleInput\(System, Names\) && !FindTargetEmitter\(Context, System, EmitterError\)\)/u);
    expect(list).toContain("TEXT(\"no emitter '%s': one of [%s]\")");
    expect(list).toContain('FindInputEmitter(System, Name, Emitter, ScopeError);');
    expect(list).toMatch(/else if \(!ScopeError\.IsEmpty\(\)\)\s*\{\s*Write\.Error = ScopeError;\s*\}/u);
    expect(list.indexOf('FindInputEmitter(')).toBeLessThan(list.indexOf('Write = WriteParameter(System, Emitter, Name, Entry);'));
    expect(list).not.toContain('ResolveEmitterHandle(');
    expect(list).not.toContain('ResolveInputEmitter(');
    expect(list).not.toContain('Nothing was changed');
    expect(sliceBetween(owner(), 'bool FindInputEmitter(', 'bool ResolveInputEmitter(')).not.toContain('Nothing was changed');
  });

  it('a type mismatch on a module input names the emitter when there is one', () => {
    const source = values();
    expect(source).toContain("FString::Printf(TEXT(\" of emitter '%s'\"), *EmitterName);");
    expect(source).toContain("TEXT(\"Module input '%s'%s is a %s; %s\"), *ParamName, *OfEmitter");
  });
});
