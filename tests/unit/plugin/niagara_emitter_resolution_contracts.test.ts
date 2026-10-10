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

  it('set_parameter_value resolves the emitter before a module input, and not for a user parameter', () => {
    const source = values();
    expect(sliceBetween(source, 'static bool NamesModuleInput(', '// One value written')).toContain('!Name.IsEmpty() && !FindUserParameter(System, Name)');
    const single = source.slice(source.indexOf('bool SetParameterValue(FActionContext& Context)'));
    const gate = 'if (NamesModuleInput(System, {ParamName}) && !ResolveEmitterHandle(Context, System))';
    expect(single.indexOf(gate)).toBeGreaterThan(-1);
    expect(single.indexOf(gate)).toBeLessThan(single.indexOf('WriteParameter(Context, System, ParamName, Context.Payload)'));
  });

  // A list fails only the entries that need the emitter: the user parameters in it are still written.
  it('a parameters list resolves the emitter once and fails only the module-input entries when it cannot', () => {
    const list = sliceBetween(values(), 'static bool SetParameterValueList(', 'bool SetParameterValue(FActionContext& Context)');
    expect(list).toMatch(/if \(NamesModuleInput\(System, Names\)\)\s*\{\s*FindTargetEmitter\(Context, System, EmitterError\);\s*\}/u);
    expect(list).toMatch(/else if \(!EmitterError\.IsEmpty\(\) && !FindUserParameter\(System, Name\)\)\s*\{\s*Write\.Error = EmitterError;\s*\}/u);
    expect(list.indexOf('FindTargetEmitter(')).toBeLessThan(list.indexOf('Write = WriteParameter(Context, System, Name, Entry);'));
    expect(list).not.toContain('ResolveEmitterHandle(');
  });

  it('a type mismatch on a module input names the emitter', () => {
    expect(values()).toContain("TEXT(\"Module input '%s' of emitter '%s' is a %s; %s\"), *ParamName, *Context.EmitterName");
  });
});
