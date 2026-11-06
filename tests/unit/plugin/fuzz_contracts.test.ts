// control_editor fuzz_blueprint (plugin Domains/Play/McpFuzz*): only a Play session this server started is called into,
// nothing reaches the Blueprint debugger, every call varies one input from the first values, and what the run hooked is
// released before the answer. Behaviour belongs to the integration cases in tests/mcp-tools/core/fuzz-blueprint.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (...parts: string[]): string =>
  readFileSync(join(PRIVATE, ...parts), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const between = (text: string, from: string, to: string): string => text.slice(text.indexOf(from), text.indexOf(to, text.indexOf(from)));

describe('fuzz_blueprint', () => {
  const handler = code('Domains', 'Play', 'McpFuzz.cpp');
  const run = code('Domains', 'Play', 'McpFuzzRun.cpp');
  const handle = handler.slice(handler.indexOf('bool Handle('));

  it('is routed by control_editor', () => {
    expect(code('Domains', 'ControlEditor', 'McpEditorDispatch.cpp')).toMatch(
      /LowerSub == TEXT\("fuzz_blueprint"\)\)\s*return McpFuzz::Handle\(\*this, RequestId, Payload, RequestingSocket\);/u,
    );
  });

  // Someone may be playing a session started in the editor: its game is not called into.
  it('calls only into a Play session this server started', () => {
    expect(handle.indexOf('McpPlaySession::StartedByMcp(World)')).toBeGreaterThan(0);
    expect(handle.indexOf('McpPlaySession::StartedByMcp(World)')).toBeLessThan(handle.indexOf('FindTarget('));
  });

  // A breakpoint or Break on Exceptions would stop the game in the debugger in the middle of a call and bring the
  // Blueprint's editor forward: breakpoints are refused before any call, the setting is held off around the calls only.
  it('keeps every call out of the Blueprint debugger', () => {
    expect(handle.indexOf('FKismetDebugUtilities::BlueprintHasBreakpoints(Blueprint)')).toBeLessThan(handle.indexOf('CallAll('));
    const guarded = between(handle, 'TGuardValue<bool> NoBreak(', '}');
    expect(guarded).toContain('GetMutableDefault<UEditorExperimentalSettings>()->bBreakOnExceptions, false');
    expect(guarded).toContain('Result = CallAll(Plans, World, Target, MaxCalls);');
  });

  // The errors are the reply's rows: the script log's copies of them were also the request's engine warnings.
  it('drops the script log\'s copies of the errors from the request\'s warnings', () => {
    const forget = between(handle, 'Bridge.ForgetCapturedMessages(', '});');
    expect(handle.indexOf('Bridge.ForgetCapturedMessages(')).toBeGreaterThan(handle.indexOf('CallAll('));
    for (const prefix of ['[LogScriptCore]', '[LogScript]']) {
      expect(forget).toContain(`Line.StartsWith(TEXT("${prefix}"))`);
    }
  });

  // The engine checks a divide by zero against LogScriptCore but its script exception handler writes it to LogScript:
  // reading only LogScriptCore caught nothing (every call of an unguarded divide read as passing).
  it('reads the script warnings in both log categories, by their prefix', () => {
    const serialize = between(run, 'virtual void Serialize(', 'private:');
    expect(serialize).toContain('Category == Script || Category == ScriptCore');
    expect(serialize).toContain('FCString::Strncmp(V, TEXT("Script Msg: "), 12) == 0');
  });

  // A device that is not safe on several threads is buffered: the log hands it the lines later, from its own thread,
  // where the game-thread check dropped every one.
  it('takes each log line as it is written, on the thread that wrote it', () => {
    expect(run).toContain('virtual bool CanBeUsedOnMultipleThreads() const override { return true; }');
  });

  it('releases the exception delegate and the log device it hooked', () => {
    const release = between(run, 'virtual ~FFuzzCapture() override', 'virtual bool CanBeUsedOnAnyThread()');
    expect(release).toContain('FBlueprintCoreDelegates::OnScriptException.Remove(Exceptions);');
    expect(release).toContain('GLog->RemoveOutputDevice(this);');
  });

  // A failing call is the smallest change from the first values: one input differs, the others keep theirs.
  it('varies one input per call and checks the run before each', () => {
    const fuzz = between(run, 'TSharedPtr<FJsonObject> Fuzz(', 'using namespace FuzzRunParts;');
    expect(fuzz).toContain('Plan.Values[Input][Input == Call.Key ? Call.Value : 0]');
    expect(fuzz.indexOf('Run.Stopped = StopReason(Run);')).toBeLessThan(fuzz.indexOf('Target->ProcessEvent(Plan.Function, Block.Data());'));
    expect(fuzz.indexOf('Run.Capture.Caught.Reset();')).toBeLessThan(fuzz.indexOf('Target->ProcessEvent(Plan.Function, Block.Data());'));
  });
});
