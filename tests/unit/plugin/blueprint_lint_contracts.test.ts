// manage_blueprint lint_blueprint: a read of every graph for the mistakes that show in Play (plugin
// Domains/BlueprintGraph/Lint). Wiring, read-only and record-parity contracts; the rules are checked live.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { LINT_RULES } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/graph-lint.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const LINT = join(PRIVATE, 'Domains', 'BlueprintGraph', 'Lint');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (path: string): string => readFileSync(path, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('blueprint lint', () => {
  const handler = code(join(LINT, 'McpBPLint.cpp'));

  it('is routed to the graph handler, after the Blueprint loads, as a read that needs no event graph', () => {
    expect(code(join(PRIVATE, 'Core', 'Subsystem', 'EngineWireSubsystemHandlerRegistration.cpp'))).toContain('TEXT("lint_blueprint")');
    const graph = code(join(PRIVATE, 'Domains', 'BlueprintGraph', 'McpBPGraphHandlers.cpp'));
    expect(graph).toMatch(/PrepareBlueprintAndGraph\(Context, \/\*bRequireGraph=\*\/bCode \? Payload->HasField\(TEXT\("graphName"\)\) : !bLint\)/u);
    expect(graph).toMatch(/if \(bLint \|\| bCode\)\s*\{\s*Context\.bDeferCompile = true;\s*return bLint \? McpBPLint::HandleLint\(Context\)/u);
  });

  it('changes nothing: no Modify, no compile, no save in any lint file', () => {
    for (const file of readdirSync(LINT)) {
      const text = code(join(LINT, file));
      for (const forbidden of ['Modify(', 'MarkBlueprintAsModified', 'CompileBlueprint', 'McpSafeAssetSave', 'FScopedTransaction']) {
        expect(text, `${file} must not call ${forbidden}`).not.toContain(forbidden);
      }
    }
  });

  it('runs exactly the rules the record lists', () => {
    const list = handler.slice(handler.indexOf('const TCHAR* const Rules[]'), handler.indexOf('};', handler.indexOf('const TCHAR* const Rules[]')));
    const cpp = [...list.matchAll(/TEXT\("([a-z_]+)"\)/gu)].map((match) => match[1]);
    expect(cpp).toEqual(LINT_RULES);
  });

  // A use is checked only when every path to it passes a check's valid exit: the other exits, a Sequence pin that skips
  // the check, an event that never runs it, all leave it unchecked.
  it('judges a use checked only past a validity check\'s valid exit that dominates it', () => {
    const guards = code(join(LINT, 'McpBPLintFlow.cpp'));
    expect(guards).toContain('CastNode->GetValidCastPin()');
    expect(guards).toContain('Get->FindPin(UEdGraphSchema_K2::PN_Then)');
    expect(guards).toContain('Macro->GetMacroGraph()->GetName() == TEXT("IsValid")');
    expect(guards).toContain('Test->GetTargetFunction()->GetFName() == IsValidFunction');
    expect(guards).toMatch(/const int32 From = Pin == ValidExit && Past != INDEX_NONE \? Past : Pair\.Value;/u);
    expect(guards).toMatch(/for \(int32 Up = Flow\.Idom\[At\]; Up > 0; Up = Flow\.Idom\[Up\]\)/u);
  });

  // GetValuePin asserts an output pin, and a Set node's variable pin is its input: calling it crashed the editor once.
  it('reads a Set node\'s value through its input pin, and an interface message to None is no use', () => {
    const guards = code(join(LINT, 'McpBPLintGuards.cpp'));
    expect(guards).toContain('Set->FindPin(Set->GetVarName(), EGPD_Input)');
    expect(guards + code(join(LINT, 'McpBPLintFlow.cpp'))).not.toContain('GetValuePin(');
    expect(guards).toContain('Node->IsA<UK2Node_Message>()');
  });

  // A node wired from a reached node is reached, so a dead chain's first node is the one with no wire into it.
  it('names only the first node of a dead chain', () => {
    const rules = code(join(LINT, 'McpBPLintRules.cpp'));
    expect(rules).toMatch(/const bool bHasCaller = Node->Pins\.ContainsByPredicate/u);
    expect(rules).toContain('if (!bHasCaller)');
  });
});
