import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// The plugin was renamed EngineWire, which moved its settings section from
// [/Script/McpAutomationBridge.McpAutomationBridgeSettings] to
// [/Script/EngineWire.EngineWireSettings]. Unreal loads config by class path and
// has no plugin-level section redirect, so without these an upgrade would silently
// drop every setting: the native MCP switch, the ports, the tokens.

const SOURCE = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire');
const header = readFileSync(resolve(SOURCE, 'Public/EngineWireSettings.h'), 'utf8');
const source = readFileSync(resolve(SOURCE, 'Private/Core/Settings/EngineWireSettings.cpp'), 'utf8');
const adopt = source.slice(source.indexOf('void UEngineWireSettings::PostInitProperties()'));
const adoptBody = adopt.slice(0, adopt.search(/\r?\n\}\r?\n/u));

describe('settings saved under the old plugin name', () => {
  it('names the old section exactly', () => {
    expect(header).toContain(
      'static constexpr const TCHAR* LegacyConfigSection = TEXT("/Script/McpAutomationBridge.McpAutomationBridgeSettings");'
    );
  });

  it('are read only when nothing sits under the current section, and only on the default object', () => {
    expect(adoptBody).toContain('if (!HasAnyFlags(RF_ClassDefaultObject) || !GConfig)');
    const current = adoptBody.indexOf('GConfig->GetSection(*GetClass()->GetPathName(), Lines, GGameIni);');
    const legacy = adoptBody.indexOf('GConfig->GetSection(LegacyConfigSection, Lines, GGameIni);');
    expect(current).toBeGreaterThan(-1);
    expect(legacy).toBeGreaterThan(current);
    expect(adoptBody.slice(current, legacy)).toContain('if (Lines.Num() > 0)');
  });

  it('replace config arrays whole, the way LoadConfig does', () => {
    expect(adoptBody).toMatch(/CastField<FArrayProperty>\(\*It\)[\s\S]*Helper\.EmptyValues\(\);[\s\S]*Helper\.AddValue\(\)/u);
    expect(adoptBody).toContain('It->HasAnyPropertyFlags(CPF_Config)');
  });

  it('are never written back by the plugin', () => {
    expect(adoptBody).not.toMatch(/SetString|SetArray|SaveConfig|Flush|TryUpdateDefaultConfigFile|EmptySection/u);
  });

  it('stay out of automation reach like the current section', () => {
    expect(header).toMatch(/SectionOrCategory\.Contains\(TEXT\("McpAutomationBridge"\), ESearchCase::IgnoreCase\)/u);
  });
});
