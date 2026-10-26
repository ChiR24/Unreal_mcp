// inspect logpoints mark Blueprint nodes to record their pin values each time they run in Play, without pausing it.
// Wiring and structure contracts only: the behaviour is covered by tests/mcp-tools/core/logpoints.test.mjs.
// (tests/unit/plugin/logpoints_contracts.test.ts)

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const LOGPOINTS = join(PRIVATE, 'Domains', 'Debug', 'Logpoints');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('logpoints', () => {
  const handler = code(LOGPOINTS, 'McpLogpoints.cpp');
  const trace = code(LOGPOINTS, 'McpLogpointsTrace.cpp');

  it('is routed by inspect for its three sub-actions', () => {
    expect(code(PRIVATE, 'Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toMatch(
      /LowerSubAction\.EndsWith\(TEXT\("_logpoints"\)\)\)\s*\{\s*return McpLogpoints::Handle\(\*this, RequestId, LowerSubAction, Payload, RequestingSocket\);/u,
    );
  });

  // Every impure node of every Blueprint in Play raises a tracepoint in the editor: a listener left bound would run on
  // each of them for the rest of the session.
  it('listens only while a node is marked', () => {
    expect(handler).toContain('Listen(true);');
    expect(handler).toContain('Listen(All.Num() > 0);');
    expect(trace).toMatch(/if \(bOn && !GExceptionHandle\.IsValid\(\)\)[\s\S]*else if \(!bOn && GExceptionHandle\.IsValid\(\)\)[\s\S]*OnScriptException\.Remove\(GExceptionHandle\)/u);
  });

  it('stops at a cast and a compare for what no marked Blueprint owns, before any lookup', () => {
    const body = trace.slice(trace.indexOf('void OnScriptException('));
    expect(body.indexOf('Points().ContainsByPredicate(OwnsIt)')).toBeLessThan(body.indexOf('GetDebugData()'));
  });

  // The editor's watch reads a function's locals only from a frame paused at a breakpoint; a logpoint never pauses.
  it('reads values through the frame that is running and never pauses the game', () => {
    expect(trace).toContain('Property->IsIn(Frame.Node) ? static_cast<const void*>(Frame.Locals)');
    expect(`${handler}\n${trace}`).not.toMatch(/Breakpoint|bDebugPauseExecution|SetPaused|GetWatchText/u);
  });

  // The wire trace is one of the node's own statements with no exec pin attached (GenerateSimpleThenGoto): looked up by
  // pin it would find nothing, and no output would ever be recorded.
  it('records only on the game thread, in Play (not Simulate), keyed by node in the class debug data', () => {
    expect(trace).toContain('if (!IsInGameThread() ||');
    expect(trace).toContain('World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game) || (GEditor && GEditor->IsSimulatingInEditor())');
    expect(trace).toContain('FindSourceNodeFromCodeLocation(Frame.Node, Offset, true)');
    expect(trace).not.toContain('FindSourcePinFromCodeLocation');
  });

  it('reads an input through reroute knots to the pin that holds its value', () => {
    expect(trace).toContain('GetPassThroughPin(Out)');
  });

  it('refuses a pure node: it has no tracepoint of its own', () => {
    expect(handler).toContain('K2->IsNodePure()');
    expect(handler).toContain('TEXT("PURE_NODE")');
  });
});
