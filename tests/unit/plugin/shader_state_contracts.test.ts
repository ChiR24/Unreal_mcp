// After `scalability 2` the next screenshot showed black trees and default-material ground because the
// shaders were still compiling, and nothing in the reply said so. Every capture reply now carries
// shadersCompiling (the outstanding GShaderCompilingManager jobs) and a warning while some are, waitForShaders
// waits for the queue on the core ticker (never on the game thread, which is what lets the compiling finish),
// and the editor-state reads carry the count. Wiring contracts only: behaviour needs an editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

import { sliceBetween } from './plugin-contract-fixtures.js';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const outputOf = (id: string) => {
  const properties = capabilityIndex().byId.get(id)?.schemas.output.properties;
  return isRecord(properties) ? properties : {};
};

describe('shader compile state: counted through the engine, waited for off the game thread', () => {
  const source = (): string => read('Foundation', 'McpScreenshotResample.cpp');
  const header = (): string => read('Foundation', 'McpScreenshotResample.h');

  it('counts the outstanding jobs through GShaderCompilingManager and caps the wait below the bridge client timeout', () => {
    expect(source()).toMatch(/int32 McpShaderJobsRemaining\(\) \{\s*return GShaderCompilingManager \? GShaderCompilingManager->GetNumRemainingJobs\(\) : 0;\s*\}/u);
    const cap = Number(/McpShaderWaitMaxSeconds = ([\d.]+);/u.exec(header())?.[1]);

    expect(cap).toBeGreaterThan(0);
    expect(cap, 'the bridge client gives up on a call at 30 s').toBeLessThan(30);
  });

  it('waits on the core ticker and never blocks the thread the compiling advances on', () => {
    const wait = sliceBetween(source(), 'bool McpDeferForShaderCompile(', '\n}\n');

    expect(wait).toContain('FTSTicker::GetCoreTicker().AddTicker(');
    expect(wait, 'FinishAllCompilation and Sleep would stall the very thread the shaders need').not.toMatch(/FinishAllCompilation|FPlatformProcess::Sleep|FlushRenderingCommands/u);
    expect(wait).toMatch(/if \(Left > 0 && Waited < McpShaderWaitMaxSeconds\) \{\s*return true;\s*\}/u);
    expect(wait).toMatch(/Wait->SetNumberField\(TEXT\("waitedSeconds"\)[^;]*;\s*Wait->SetNumberField\(TEXT\("jobsLeft"\), Left\);\s*Wait->SetBoolField\(TEXT\("timedOut"\), Left > 0\);\s*Payload->SetObjectField\(TEXT\("shaderWait"\), Wait\);\s*Resume\(Payload\);\s*return false;/u);
  });

  it('defers only when asked, when something is compiling, and once', () => {
    const wait = sliceBetween(source(), 'bool McpDeferForShaderCompile(', 'const double Start');

    expect(wait).toMatch(/TryGetBoolField\(TEXT\("waitForShaders"\), bWait\) \|\| !bWait \|\|\s*Payload->HasField\(TEXT\("shaderWait"\)\) \|\| McpShaderJobsRemaining\(\) == 0\) \{\s*return false;/u);
  });

  it('adds shadersCompiling to every reply, a warning while some compile (keeping the ones already there), and the wait it ran', () => {
    const add = sliceBetween(source(), 'void McpAddShaderCompileState(', 'bool McpDeferForShaderCompile(');

    expect(add).toContain('Resp->SetNumberField(TEXT("shadersCompiling"), Jobs);');
    expect(add).toMatch(/TryGetObjectField\(TEXT\("shaderWait"\), Waited\)[\s\S]*?Resp->SetObjectField\(TEXT\("shaderWait"\), \*Waited\);/u);
    expect(add).toMatch(/if \(Jobs > 0\) \{[\s\S]*?TryGetArrayField\(TEXT\("warnings"\), Existing\)[\s\S]*?Warnings = \*Existing;[\s\S]*?Warnings\.Add\([\s\S]*?Resp->SetArrayField\(TEXT\("warnings"\), Warnings\);/u);
    expect(add).toContain('Retry with waitForShaders');
  });
});

describe('every screenshot surface reports it, and control_editor.screenshot can wait', () => {
  it('the editor capture holds the call before it picks a mode, and resumes the same handler once the queue is done', () => {
    const source = read('Domains', 'ControlEditor', 'McpAutomationBridge_ControlEditorScreenshot.cpp');

    expect(source).toMatch(/if \(McpDeferForShaderCompile\(\s*Payload, \[Weak = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>\(this\), RequestId,\s*Socket\]\(const TSharedPtr<FJsonObject> &Waited\) \{\s*if \(UMcpAutomationBridgeSubsystem \*Self = Weak\.Get\(\)\) \{\s*Self->HandleControlEditorScreenshot\(RequestId, Waited, Socket\);\s*\}\s*\}\)\) \{\s*return true;\s*\}/u);
    expect(source.indexOf('McpDeferForShaderCompile('), 'before game_viewport forwards and before the mode is read').toBeLessThan(source.indexOf('FString Mode;'));
  });

  it('the editor-viewport and window receipt, and the game-viewport reply, add the state', () => {
    expect(read('Domains', 'ControlEditor', 'McpAutomationBridge_ControlEditorScreenshotSupport.cpp'))
      .toMatch(/AddScreenshotMetadataForMcp\(Resp, Payload\);\s*McpAddShaderCompileState\(Resp, Payload\);/u);
    expect(read('Domains', 'Ui', 'McpAutomationBridge_UiHandlersScreenshot.cpp'))
      .toMatch(/AddScreenshotMetadataForUiMcp\(Resp, Payload\);\s*McpAddShaderCompileState\(Resp, Payload\);/u);
  });

  it('each editor-state read (project, editor and world settings, viewport, selection) carries the count before it answers', () => {
    const source = read('Domains', 'Environment', 'Inspection', 'McpAutomationBridge_EnvironmentHandlersInspectSettings.cpp');

    for (const action of ['get_project_settings', 'get_editor_settings', 'get_world_settings', 'get_viewport_info', 'get_selected_actors']) {
      const branch = sliceBetween(source, `LowerSubAction.Equals(TEXT("${action}"))`, 'return true;');

      expect(branch, action).toMatch(/McpAddShaderCompileState\(Resp, nullptr\);\s*Resp->SetBoolField\(TEXT\("success"\), true\);/u);
    }
    expect(source.match(/McpAddShaderCompileState\(/gu), 'one per read, none on the stats reads that follow').toHaveLength(5);
  });

  it('the records declare waitForShaders and where the count and the wait come back', () => {
    const input = capabilityIndex().byId.get('control_editor.screenshot')?.schemas.input.properties;
    const wait = isRecord(input) ? input.waitForShaders : undefined;
    const output = outputOf('control_editor.screenshot');

    expect(isRecord(wait) ? wait.type : undefined).toBe('boolean');
    expect(isRecord(wait) ? wait.description : '').toMatch(/at most 25 seconds.*shaderWait/u);
    expect(isRecord(output.shadersCompiling) ? output.shadersCompiling.type : undefined).toBe('number');
    expect(isRecord(output.shaderWait) ? output.shaderWait.description : '').toMatch(/waitedSeconds, jobsLeft .* and timedOut/u);
    expect(isRecord(outputOf('inspect.get_editor_state').shadersCompiling)).toBe(true);
  });
});
