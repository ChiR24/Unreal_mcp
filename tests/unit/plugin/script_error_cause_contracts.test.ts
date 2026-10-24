// Why an Accessed None fired: McpScriptErrorCause reads the variable the engine names and the Blueprints that assign
// it. Wiring and version contracts only; the causes are checked live through runtime_report.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const LOG = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Log');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(LOG, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('Accessed None root cause', () => {
  const cause = code('McpScriptErrorCause.cpp');
  const errors = code('McpScriptErrors.cpp');

  // A session can raise one error thousands of times: the cause is read once, when it is first seen.
  it('is read on an error\'s first hit only, never per hit', () => {
    const firstHit = errors.slice(errors.indexOf('ByCodeLocation.Add(Key, Errors.Num());'), errors.indexOf('void FMcpScriptErrors::Record('));
    expect(firstHit).toContain('Error.Cause = McpScriptErrorCause::Diagnose(ActiveObject, StackFrame, Info);');
    const record = errors.slice(errors.indexOf('void FMcpScriptErrors::Record('), errors.indexOf('TArray<FString> FMcpScriptErrors::LinesSince('));
    expect(record).not.toContain('Diagnose(');
  });

  it('only diagnoses an Accessed None, from a variable the class itself owns', () => {
    expect(cause).toContain('Info.GetType() != EBlueprintExceptionType::AccessViolation');
    // A local variable's owner is its function; GetOwnerClass would walk past it to the class.
    expect(cause).toContain('Property->GetOwner<UClass>()');
    expect(cause).not.toContain('GetOwnerClass()');
    // FFrame::MostRecentPropertyContainer does not exist in UE 5.0.
    expect(cause).not.toContain('MostRecentPropertyContainer');
  });

  it('reports the cause in runtime_report rows and in the warning line', () => {
    expect(errors).toMatch(/if \(Error\.Cause\.IsValid\(\)\)\s*\{\s*Entry->SetObjectField\(TEXT\("cause"\), Error\.Cause\);/u);
    expect(errors).toContain('Error.Cause->GetStringField(TEXT("detail"))');
  });

  // GetValuePin asserts an output pin (a Get node's): on a Set node it took the editor down at the first Accessed None.
  it('reads a Set node\'s value from its input pin, never through GetValuePin', () => {
    expect(cause).toContain('Set->FindPin(Set->GetVarName(), EGPD_Input)');
    expect(cause).not.toContain('GetValuePin(');
  });

  it('names a kind for every way it can end', () => {
    for (const kind of ['destroyed', 'instance_editable_unset', 'never_set', 'cast_failed', 'function_returned_none', 'set_but_none', 'other']) {
      expect(cause).toContain(`TEXT("${kind}")`);
    }
  });
});
