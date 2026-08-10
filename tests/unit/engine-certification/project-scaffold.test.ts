// tests/unit/engine-certification/project-scaffold.test.ts
// Task 52 — the generated project, checked where it genuinely varies.
//
// Most of the scaffold is a constant string and testing a constant against
// itself proves nothing. Two things are real logic, and both fail in ways that
// are expensive to diagnose from an engine's own error output:
//
//   EngineIncludeOrderVersion does not exist before 5.1. Emitting it there is a
//   C# compile error inside UBT, surfacing as "target rules failed to compile"
//   with no mention of the line responsible.
//
//   The plugin's settings object is `config=Game`. Writing its section into
//   DefaultEngine.ini parses cleanly, is ignored, and looks exactly like the
//   plugin failing to start — the same symptom as a broken build.

import { describe, expect, it } from 'vitest';

import { defaultGameIni, scaffoldFiles, targetFile, uprojectFile } from './project-scaffold.mjs';

const engine = (minor: number) => ({ major: 5, minor, patch: 0 });

describe('targetFile', () => {
  it('omits EngineIncludeOrderVersion on 5.0, where the enum does not exist', () => {
    const rules = targetFile({ name: 'McpCert', kind: 'Editor', engine: engine(0) });
    expect(rules).not.toContain('IncludeOrderVersion');
    expect(rules).toContain('Type = TargetType.Editor;');
  });

  it('emits it from 5.1 up', () => {
    for (const minor of [1, 3, 5, 7, 8]) {
      expect(targetFile({ name: 'McpCert', kind: 'Editor', engine: engine(minor) }), `5.${minor}`)
        .toContain('IncludeOrderVersion = EngineIncludeOrderVersion.Latest;');
    }
  });

  it('names the class the way UBT requires for each target kind', () => {
    expect(targetFile({ name: 'McpCert', kind: 'Game', engine: engine(7) })).toContain('public class McpCertTarget : TargetRules');
    expect(targetFile({ name: 'McpCert', kind: 'Editor', engine: engine(7) })).toContain('public class McpCertEditorTarget : TargetRules');
  });
});

describe('uprojectFile', () => {
  it('declares the module and enables the bridge plugin', () => {
    const parsed = JSON.parse(uprojectFile('McpCert'));
    expect(parsed.Modules).toEqual([{ Name: 'McpCert', Type: 'Runtime', LoadingPhase: 'Default' }]);
    expect(parsed.Plugins).toEqual([{ Name: 'McpAutomationBridge', Enabled: true }]);
  });
});

describe('defaultGameIni', () => {
  const ini = defaultGameIni({ nativePort: 41234, wsPorts: [41235, 41236] });

  it('writes the plugin settings under the section the plugin actually reads', () => {
    expect(ini).toContain('[/Script/McpAutomationBridge.McpAutomationBridgeSettings]');
  });

  it('carries the ports this run allocated, not the wave-wide defaults', () => {
    expect(ini).toContain('NativeMCPPort=41234');
    expect(ini).toContain('ListenPorts=41235,41236');
    expect(ini).not.toContain('3000');
    expect(ini).not.toContain('8090');
  });

  it('pins the LAN flags to False rather than omitting them', () => {
    // Omitting them would leave a future default flip free to expose a run.
    expect(ini).toContain('bAllowNonLoopback=False');
    expect(ini).toContain('ListenHost=127.0.0.1');
  });
});

describe('scaffoldFiles', () => {
  it('produces exactly the files UBT needs for a C++ project, and no others', () => {
    const files = scaffoldFiles({ name: 'McpCert', engine: engine(7), nativePort: 1, wsPorts: [2, 3] });
    expect(Object.keys(files).sort()).toEqual([
      'Config/DefaultEngine.ini',
      'Config/DefaultGame.ini',
      'McpCert.uproject',
      'Source/McpCert.Target.cs',
      'Source/McpCert/McpCert.Build.cs',
      'Source/McpCert/McpCert.cpp',
      'Source/McpCertEditor.Target.cs',
    ]);
  });

  it('puts the primary game module macro in the module source', () => {
    const files = scaffoldFiles({ name: 'McpCert', engine: engine(7), nativePort: 1, wsPorts: [2, 3] });
    expect(files['Source/McpCert/McpCert.cpp']).toContain('IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, McpCert, "McpCert");');
  });
});
