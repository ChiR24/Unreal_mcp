// lint_blueprint's rules also run as a Data Validation validator (module EngineWireValidation), so Validate Assets, saves
// and validation commandlets report them too. Its wiring can only be read here: the validator needs an editor to run.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PLUGIN = join('plugins', 'EngineWire');
const read = (...segments: readonly string[]): string => readFileSync(join(PLUGIN, ...segments), 'utf8');

describe('the lint validator module', () => {
  // DataValidation is an optional plugin a project may turn off; a module importing it would then fail to load and take
  // the plugin's startup with it. So the .uplugin never loads it, and EngineWire loads it only where DataValidation is on.
  it('is an editor module of the plugin that only EngineWire loads, and only where DataValidation exists', () => {
    const plugin = JSON.parse(read('EngineWire.uplugin')) as {
      Modules: { Name: string; Type: string; LoadingPhase: string }[];
      Plugins: { Name: string; Enabled: boolean }[];
    };
    expect(plugin.Modules).toContainEqual(expect.objectContaining({ Name: 'EngineWireValidation', Type: 'Editor', LoadingPhase: 'None' }));
    expect(plugin.Plugins).toContainEqual(expect.objectContaining({ Name: 'DataValidation', Enabled: true }));
    const rules = read('Source', 'EngineWireValidation', 'EngineWireValidation.Build.cs');
    expect(rules).toContain('"DataValidation", "EngineWire"');
    const startup = read('Source', 'EngineWire', 'Private', 'Core', 'Module', 'EngineWireModule.cpp');
    expect(startup).toContain('if (FModuleManager::Get().ModuleExists(TEXT("DataValidation")) && FModuleManager::Get().LoadModule(TEXT("DataValidation")))');
    expect(startup).toContain('FModuleManager::Get().LoadModule(TEXT("EngineWireValidation"));');
  });

  // 5.4 replaced the overrides; the old pair still runs there but through a path that fires an ensure.
  it('overrides the validator pair each engine calls', () => {
    const header = read('Source', 'EngineWireValidation', 'Private', 'McpLintValidator.h');
    const guard = header.indexOf('#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)');
    const otherwise = header.indexOf('#else', guard);
    expect(guard).toBeGreaterThan(-1);
    expect(header.indexOf('FDataValidationContext& Context) override;')).toBeGreaterThan(guard);
    expect(header.indexOf('FDataValidationContext& Context) override;')).toBeLessThan(otherwise);
    expect(header.indexOf('TArray<FText>& ValidationErrors) override;')).toBeGreaterThan(otherwise);
  });

  it('reports what lint_blueprint finds, through the same rule runner', () => {
    expect(read('Source', 'EngineWire', 'Public', 'McpBlueprintLint.h')).toContain('ENGINEWIRE_API TArray<FMcpLintIssue> McpLintBlueprint(const UBlueprint* Blueprint);');
    const lint = read('Source', 'EngineWire', 'Private', 'Domains', 'BlueprintGraph', 'Lint', 'McpBPLint.cpp');
    expect(lint).toContain('const TArray<UEdGraph*> Graphs = LintRunRules(Context.Blueprint, On, Findings);');
    expect(lint).toContain('McpBPLint::LintRunRules(Blueprint, [](const TCHAR*) { return true; }, Findings);');
    const validator = read('Source', 'EngineWireValidation', 'Private', 'McpLintValidator.cpp');
    expect(validator.match(/McpLintBlueprint\(Cast<UBlueprint>\(InAsset\)\)/gu)).toHaveLength(2);
    expect(validator).toContain('if (Issue.Severity == TEXT("error"))');
  });
});
