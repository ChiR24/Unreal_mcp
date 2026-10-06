// Which emitter (and system) a Niagara authoring edit acts on.
//
// Measured live on UE 5.8 against dev 591d3b0e: on a system with one emitter, a module, a dynamic input or a
// parameter binding sent with an emitterName the system does not have (or "DefaultEmitter") landed in the sole
// emitter and answered success; set_parameter_value without emitterName wrote the module input in every emitter
// that has it; systemPath and assetPath naming two systems edited systemPath and answered success.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const DIR = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'NiagaraAuthoring');
const code = (file: string): string =>
  readFileSync(join(DIR, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const context = (): string => code('McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');
const values = (): string => code('McpAutomationBridge_NiagaraAuthoringHandlersParameterValues.cpp');

describe('Niagara emitter resolution', () => {
  it('falls back to the sole emitter only for an omitted emitterName, and says so', () => {
    const resolve = sliceBetween(context(), 'FNiagaraEmitterHandle* ResolveEmitterHandle(', 'bool LoadSystemAndEmitter(');
    expect(resolve).toMatch(/if \(!Handle && Context\.EmitterName\.IsEmpty\(\) && Handles\.Num\(\) == 1\)/u);
    expect(resolve).toContain('TEXT("emitterResolvedBy"), TEXT("single-emitter-fallback")');
    // A chosen name that misses must not reach the fallback by another route.
    expect(resolve.match(/Handles\[0\]/gu) ?? []).toHaveLength(1);
  });

  it('refuses a missing emitter naming the system, its emitters and that nothing changed', () => {
    const resolve = sliceBetween(context(), 'FNiagaraEmitterHandle* ResolveEmitterHandle(', 'bool LoadSystemAndEmitter(');
    expect(resolve).toContain('TEXT("EMITTER_NOT_FOUND")');
    expect(resolve).toContain('(its emitters: [%s])');
    expect(resolve).toContain('Nothing was changed.');
    expect(resolve).toContain('System->GetPathName()');
    expect(resolve).toMatch(/return nullptr;\s*\}/u);
  });

  it('names the system and emitter a successful edit acted on', () => {
    const resolve = sliceBetween(context(), 'FNiagaraEmitterHandle* ResolveEmitterHandle(', 'bool LoadSystemAndEmitter(');
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

  it('set_parameter_value resolves the emitter before a module input, and not for a user parameter', () => {
    const source = values();
    const gate = sliceBetween(source, 'static bool ResolveEmitterForModuleInputs(', '// One value written');
    expect(gate).toContain('!Name.IsEmpty() && !FindUserParameter(System, Name)');
    expect(gate).toContain('ResolveEmitterHandle(Context, System) != nullptr');
    const single = sliceBetween(source, 'bool SetParameterValue(FActionContext& Context)', '\n}\n}');
    expect(single.indexOf('ResolveEmitterForModuleInputs(Context, System, {ParamName})')).toBeGreaterThan(-1);
    expect(single.indexOf('ResolveEmitterForModuleInputs(Context, System, {ParamName})'))
      .toBeLessThan(single.indexOf('WriteParameter(Context, System, ParamName, Context.Payload)'));
    expect(single).toContain('return !ResolveEmitterForModuleInputs(Context, System, Names) || SetParameterValueList(Context, System, *Entries);');
  });

  it('a type mismatch on a module input names the emitter', () => {
    expect(values()).toContain("TEXT(\"Module input '%s' of emitter '%s' is a %s; %s\"), *ParamName, *Context.EmitterName");
  });
});
