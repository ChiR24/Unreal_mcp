import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

// StructUtils merged into CoreUObject in UE 5.5 and its headers moved under a
// StructUtils/ prefix. The engine kept forwarding shims at the old paths behind
// UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5 and then DELETED them in 5.8:
//
//   UE 5.0-5.4  "Engine/UserDefinedStruct.h" / "InstancedStruct.h"
//   UE 5.5-5.7  both the StructUtils/ paths and the old shims resolve
//   UE 5.8+     ONLY the StructUtils/ paths exist
//
// Five files included the StructUtils/ paths unconditionally, so the plugin
// could not compile on 5.0-5.4 at all despite advertising 5.0-5.8 support. The
// raw path builds fine on every engine installed for development, which is
// exactly why this needs a contract rather than a build to catch it.
describe('plugin engine-version compatibility contracts', () => {
  const STRUCT_SITES: ReadonlyArray<readonly [string[], string]> = [
    [['Domains', 'AssetWorkflow', 'DataTables', 'Shared.h'], 'MCP_USER_DEFINED_STRUCT_HEADER'],
    [
      ['Domains', 'AssetWorkflow', 'Structs', 'McpAutomationBridge_AssetWorkflowStructsShared.h'],
      'MCP_USER_DEFINED_STRUCT_HEADER',
    ],
    [
      ['Domains', 'Blueprint', 'Graph', 'McpAutomationBridge_BlueprintHandlersStructMakeBreak.cpp'],
      'MCP_USER_DEFINED_STRUCT_HEADER',
    ],
    [['Domains', 'Inspect', 'McpAutomationBridge_InspectStruct.cpp'], 'MCP_USER_DEFINED_STRUCT_HEADER'],
    [
      ['Domains', 'StructProperty', 'McpAutomationBridge_StructPropertyInstanced.cpp'],
      'MCP_INSTANCED_STRUCT_HEADER',
    ],
  ];

  it('reaches StructUtils headers through the version macro, never a raw 5.5+ path', () => {
    for (const [parts, macro] of STRUCT_SITES) {
      const source = pluginSource(...parts);

      expect(source).toContain(`#include ${macro}`);
      expect(source).toContain('Core/Compatibility/McpVersionCompatibility.h');
      // The raw path is the regression: it resolves on 5.5+ and fails on 5.0-5.4.
      expect(source).not.toMatch(/#include\s+"StructUtils\//);
    }
  });

  it('probes the StructUtils header location with __has_include, not a version number', () => {
    const compat = pluginSource('Core', 'Compatibility', 'McpVersionCompatibility.h');

    // __has_include keeps this correct if Epic moves the header again; an
    // ENGINE_MINOR_VERSION comparison would have to be re-audited every release.
    expect(compat).toMatch(/__has_include\("StructUtils\/UserDefinedStruct\.h"\)/);
    expect(compat).toMatch(/__has_include\("StructUtils\/InstancedStruct\.h"\)/);
    expect(compat).toContain('#define MCP_USER_DEFINED_STRUCT_HEADER "StructUtils/UserDefinedStruct.h"');
    expect(compat).toContain('#define MCP_USER_DEFINED_STRUCT_HEADER "Engine/UserDefinedStruct.h"');
    expect(compat).toContain('#define MCP_INSTANCED_STRUCT_HEADER "StructUtils/InstancedStruct.h"');
    expect(compat).toContain('#define MCP_INSTANCED_STRUCT_HEADER "InstancedStruct.h"');
  });

  // Verified against the installed engines: UE 5.7 declares
  //   SetEnums(Names, CppForm, Flags = None, bAddMaxKeyIfMissing = true)
  // so a 2-argument call binds, while UE 5.8 declares
  //   SetEnums(Names, CppForm, UnderlyingType, Flags, EAddMaxKeyIfMissing)
  // with no defaults, so the 2-argument call no longer compiles.
  it('keeps the UUserDefinedEnum::SetEnums arity split at the 5.8 boundary', () => {
    const compat = pluginSource('Core', 'Compatibility', 'McpVersionCompatibility.h');

    expect(compat).toMatch(/ENGINE_MINOR_VERSION >= 8[\s\S]*?#define MCP_SET_ENUMS/);
    expect(compat).toContain('GetUnderlyingType()');
    expect(compat).toContain('UEnum::EAddMaxKeyIfMissing::Yes');
  });

  // Each boundary below was read out of the engine headers at the release tags
  // in EpicGames/UnrealEngine, not inferred. All four were previously wrong in
  // the same direction — they enabled a modern API one or more versions before
  // it existed, and every one of them made UE 5.1 call something that is not
  // there. Guarding the numbers keeps a plausible-looking "off by one" edit
  // from silently reintroducing a version-specific compile break that no build
  // on this machine can catch.
  it('pins the version boundaries that were verified against engine source', () => {
    const compat = pluginSource('Core', 'Compatibility', 'McpVersionCompatibility.h');

    // WidgetVariableNameToGuidMap: absent 5.0-5.5, present from 5.6.0.
    expect(compat).toMatch(
      /ENGINE_MINOR_VERSION >= 6\s*\n\s*#define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 1/,
    );
    // UIKRigDefinitionFactory::CreateNewIKRigAsset: absent 5.0-5.5, present from 5.6.0.
    expect(compat).toMatch(
      /ENGINE_MINOR_VERSION >= 6\s*\n\s*#define MCP_HAS_IKRIG_CREATE_NEW_ASSET 1/,
    );
    // FAssetCompilingManager::FinishCompilationForObjects: absent 5.0-5.1, present from 5.2.
    expect(compat).toMatch(
      /ENGINE_MINOR_VERSION >= 2\s*\n#define MCP_HAS_FINISH_COMPILATION_FOR_OBJECTS 1/,
    );
    // IKRetargeterController::SetIKRig(enum) replaces SetSourceIKRig/SetTargetIKRig in 5.2.
    expect(compat).toMatch(
      /ENGINE_MINOR_VERSION >= 2\s*\n\s*#define MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM 1/,
    );

    // The pre-5.2 pair must remain reachable — it is what 5.0/5.1 actually have.
    expect(compat).toContain('(Controller)->SetSourceIKRig(Rig)');
    expect(compat).toContain('(Controller)->SetTargetIKRig(Rig)');
  });
});
