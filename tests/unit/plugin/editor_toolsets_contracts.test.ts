/**
 * Source contracts for Epic's editor toolsets behind the gateway (UE 5.8 Toolset Registry). Epic's own MCP server has no
 * authentication and no consent; these tools reach the editor through this plugin's scopes and consent instead, so the
 * rules that keep that true are pinned here: the registry is reached by reflection only (nothing links an experimental
 * plugin, and 5.0-5.7 load unchanged), a destructive tool needs the destructive door, and a script or console tool never
 * runs. The C++ cannot run here.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const source = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const read = (file: string): string => readFileSync(resolve(source, file), 'utf8');
const code = (file: string): string => read(file).replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('Epic editor toolsets behind this plugin\'s gateway', () => {
  const list = code('Private/Domains/ControlEditor/Toolsets/McpAutomationBridge_EditorToolsets.cpp');
  const call = code('Private/Domains/ControlEditor/Toolsets/McpAutomationBridge_EditorToolsetsCall.cpp');

  it('reaches the registry through reflection only, never by linking it', () => {
    expect(list).toContain('FindObject<UClass>(nullptr, TEXT("/Script/ToolsetRegistry.ToolsetRegistry"))');
    expect(call).toContain('FindFunctionByName(TEXT("ExecuteTool"))');
    expect(read('McpAutomationBridge.Build.cs')).not.toContain('"ToolsetRegistry"');
    expect(read('../../McpAutomationBridge.uplugin')).not.toContain('"ToolsetRegistry"');
  });

  it('never calls a tool that would run scripts or console code', () => {
    expect(list).toMatch(/Lower\.Contains\(TEXT\("python"\)\) \|\| Lower\.Contains\(TEXT\("console"\)\) \|\| Lower\.Contains\(TEXT\("shell"\)\)\)\s*\{\s*return EToolClass::Blocked;/u);
    expect(call.indexOf('TEXT("EDITOR_TOOL_BLOCKED")')).toBeLessThan(call.indexOf('StartCall(Toolset, Tool, InputJson, Error)'));
  });

  it('never calls ConfigSettingsToolset, which reaches this plugin\'s own settings and token', () => {
    expect(list).toMatch(/Lower\.StartsWith\(TEXT\("configsettingstoolset\."\)\)\)\s*\{\s*return EToolClass::Blocked;/u);
  });

  it('refuses a destructive tool on the write door before it starts', () => {
    expect(call).toContain('if (Class == EToolClass::Destructive && !bDestructiveAllowed)');
    expect(call.indexOf('TEXT("DESTRUCTIVE_EDITOR_TOOL")')).toBeLessThan(call.indexOf('StartCall(Toolset, Tool, InputJson, Error)'));
    const dispatch = code('Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorDispatch.cpp');
    expect(dispatch).toContain('LowerSub == TEXT("call_editor_tool_destructive"));');
  });

  it('holds Epic\'s pending result until it settles, since only Epic\'s own callback keeps it alive', () => {
    expect(call).toContain('MakeShared<TStrongObjectPtr<UObject>>(Pending)');
    expect(call).toContain('FindFProperty<FBoolProperty>(Class, TEXT("bIsComplete"))');
  });
});
