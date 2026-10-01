// inspect.set_property on a Blueprint CDO wrote to a default object that the compile it then ran had already replaced:
// a variable the Blueprint declares got its value back from its DefaultValue text, so the write was gone, while the reply
// read the value, the path and the class off the replaced copy (/Engine/Transient, REINST_<Class>_N, saved false) and
// said success. Each call compiled again, so the next one landed on REINST_..._N+1. Wiring contracts only; behaviour
// needs an editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

const PROPERTY = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'Property');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(PROPERTY, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
/** One spaced line, with the string literals C++ concatenates joined, so a message split over lines is one string. */
const compact = (source: string): string => source.split(/\s+/u).join(' ').split('" "').join('');

const setter = (): string => code('McpAutomationBridge_PropertyHandlersObjectSet.cpp');
const target = (): string => code('McpAutomationBridge_PropertyHandlersTarget.cpp');

describe('set_property on a Blueprint CDO reaches the Blueprint\'s current default object', () => {
  it('a Default__ objectPath of a class a compile replaced resolves to the Blueprint\'s current default object', () => {
    const flat = compact(target());

    expect(flat).toContain('Out.Blueprint = UBlueprint::GetBlueprintFromClass(Out.RootObject->GetClass());');
    expect(flat).toContain('UClass* Current = Out.Blueprint ? static_cast<UClass*>(Out.Blueprint->GeneratedClass) : nullptr; if (Current && Current != Out.RootObject->GetClass()) { Out.RootObject = Current->GetDefaultObject(); Out.ObjectPath = Out.RootObject->GetPathName(); }');
  });

  it('a variable the Blueprint declares keeps the written value as its default, so the compile does not undo it', () => {
    const keep = compact(sliceBetween(target(), 'void KeepDeclaredDefault(', 'bool FindOnCurrentDefault('));
    const flat = compact(setter());
    const apply = flat.indexOf('ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError)');
    const kept = flat.indexOf('McpPropertyTarget::KeepDeclaredDefault(ResolvedBlueprint, RootObject, ResolvedPath);');
    const compile = flat.indexOf('FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);');

    expect(keep).toContain('const FString Head = Dot == INDEX_NONE ? PropertyPath : PropertyPath.Left(Dot);');
    expect(keep, 'a member of a declared struct is a change to the whole variable').toContain('FindPropertyByName(FName(*Head))');
    expect(keep).toContain('FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Variable->GetFName())');
    expect(keep).toContain('Blueprint->NewVariables[Index].DefaultValue = Text;');
    expect(apply).toBeGreaterThan(-1);
    expect(kept, 'after the write').toBeGreaterThan(apply);
    expect(compile, 'and before the compile that would have undone it').toBeGreaterThan(kept);
  });

  it('only a default object is synced and read back: a component template keeps its own path', () => {
    const flat = compact(setter());

    expect(flat).toContain('const bool bDefaultObject = ResolvedBlueprint && RootObject->HasAnyFlags(RF_ClassDefaultObject);');
    expect(flat).toContain('const bool bCompare = bDefaultObject && !Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference);');
  });

  it('after the compile the new default object is found again, and it is the one the reply, the value and the save describe', () => {
    const flat = compact(setter());
    const compile = flat.indexOf('FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);');
    const refresh = flat.indexOf('McpPropertyTarget::FindOnCurrentDefault(ResolvedBlueprint, ResolvedPath, RootObject, Property, TargetContainer)');
    const owning = flat.indexOf('UPackage* OwningPackage = RootObject->GetOutermost();');
    const verification = flat.indexOf('McpHandlerUtils::AddVerification(ResultPayload, RootObject);');
    const value = flat.indexOf('McpPropertyReflection::ExportPropertyToJsonValue(TargetContainer, Property)');

    expect(compile).toBeGreaterThan(-1);
    expect(refresh, 'after the compile').toBeGreaterThan(compile);
    expect(owning, 'the saved package is the new default object\'s').toBeGreaterThan(refresh);
    expect(verification, 'the reply describes the new default object').toBeGreaterThan(refresh);
    expect(value, 'the value is read off the new default object').toBeGreaterThan(refresh);
    expect(flat, 'a watch on the written object follows it').toContain('if (bWatch && Watch.Object.Get() == Replaced) { Watch.Object = RootObject; }');

    const find = compact(sliceBetween(target(), 'bool FindOnCurrentDefault(', 'bool ResolvePropertyTarget('));
    expect(find).toContain('UClass* Current = Blueprint->GeneratedClass; UObject* Fresh = Current ? Current->GetDefaultObject() : nullptr;');
    expect(find).toContain('McpResolvePropertyPath(Fresh, PropertyPath, Container, ResolvedPath, Error)');
  });

  it('a value the compile did not keep is an error naming both values, never a success', () => {
    const flat = compact(sliceBetween(setter(), 'FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);', 'RefreshK2NodeTitleCacheIfNeeded('));

    expect(flat).toContain('MCP_PROPERTY_EXPORT_TEXT(Property, KeptText, Property->ContainerPtrToValuePtr<void>(TargetContainer), nullptr, nullptr, PPF_None);');
    expect(flat).toContain('if (!bFound || (bCompare && KeptText != WrittenText)) {');
    expect(flat).toContain('TEXT("PROPERTY_SET_FAILED")');
    expect(flat).toContain("The Blueprint compile did not keep the write: '%s' reads '%s' on the Blueprint's default object afterwards, not '%s'.");
    const all = compact(setter());
    expect(all.indexOf('MCP_PROPERTY_EXPORT_TEXT(Property, WrittenText,'), 'the written value is taken before the compile').toBeLessThan(all.indexOf('FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);'));
  });
});

describe('set_property refuses what a Blueprint compile left behind', () => {
  it('a REINST_/SKEL_/TRASHCLASS_ class, a replaced class, or a Blueprint object in the transient package with no game running', () => {
    const flat = compact(sliceBetween(target(), 'bool IsSupersededTarget(', 'void KeepDeclaredDefault('));

    expect(flat).toContain('Class->HasAnyClassFlags(CLASS_NewerVersionExists)');
    expect(flat).toContain('ClassName.StartsWith(TEXT("REINST_"))');
    expect(flat).toContain('ClassName.StartsWith(TEXT("SKEL_"))');
    expect(flat).toContain('ClassName.StartsWith(TEXT("TRASHCLASS_"))');
    expect(flat).toContain('return UBlueprint::GetBlueprintFromClass(Class) && Object->GetOutermost() == GetTransientPackage() && !(GEditor && GEditor->PlayWorld);');
  });

  it('answers STALE_TARGET before any write, and names the way out', () => {
    const source = setter();
    const flat = compact(source);

    expect(flat).toContain('if (McpPropertyTarget::IsSupersededTarget(RootObject)) {');
    expect(flat).toContain('TEXT("STALE_TARGET")');
    expect(flat).toContain('Address the Blueprint itself with blueprintPath (its current default object), or the live actor or asset.');
    expect(source.indexOf('IsSupersededTarget(RootObject)'), 'before the first write').toBeLessThan(source.indexOf('RootObject->Modify();'));
  });
});

describe('the set_property record says where a Blueprint write lands', () => {
  it('blueprintCompiled describes the read-back, the saved package, the kept default and both refusals', () => {
    const properties = capabilityIndex().byId.get('inspect.set_property')?.schemas.output.properties;
    const entry = isRecord(properties) ? properties.blueprintCompiled : undefined;
    const description = isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';

    expect(description).toMatch(/read back from the recompiled class default object and the Blueprint package is saved/u);
    expect(description).toMatch(/a variable the Blueprint declares keeps the value as its default/u);
    expect(description).toMatch(/PROPERTY_SET_FAILED, never success/u);
    expect(description).toMatch(/Default__ objectPath resolves to the Blueprint's current default object/u);
    expect(description).toMatch(/STALE_TARGET/u);
  });
});
