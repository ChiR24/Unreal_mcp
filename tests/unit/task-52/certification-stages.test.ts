// tests/unit/task-52/certification-stages.test.ts
// Task 52 — the failure injections, run offline.
//
// A certification chain is only worth anything if each link refuses when it
// should. These are the four refusals the plan names, each reproduced here
// without an engine so they are checked on every commit rather than once, by
// hand, on the day someone remembers:
//
//   wrong root       — resolving a minor that is absent, or present but not
//                      runnable, must refuse rather than fall through to "some
//                      engine".
//   stale binary     — a compiled .so older than the sources it came from
//                      certifies yesterday's plugin under today's heading.
//   port collision   — covered in disposable-project.test.ts, where the port is
//                      really taken by a real listener.
//   truncated run    — Unreal stops scheduling after an ensure and still exits
//                      tidily. 214 started / 4 completed is a FAILED run that
//                      reads like a fast one, and only started==completed sees it.

import { describe, expect, it } from 'vitest';
import { mkdtempSync, mkdirSync, utimesSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { judgeBinaryFreshness, parseAutomationLog, portAnswers, sha256File } from './certification-stages.mjs';
import { buildEngineInventory } from './engine-inventory.mjs';

/** A throwaway plugin tree with a binary and one source file. */
function fakePluginTree(options: { binaryOlderBySeconds?: number } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'task52-freshness-'));
  const sourceRoot = join(root, 'Source');
  mkdirSync(sourceRoot, { recursive: true });
  const source = join(sourceRoot, 'Thing.cpp');
  writeFileSync(source, '// source');
  const binary = join(root, 'libUnrealEditor-McpAutomationBridge.so');
  writeFileSync(binary, 'ELF');
  const now = Date.now() / 1000;
  const skew = options.binaryOlderBySeconds ?? 0;
  utimesSync(binary, now - skew, now - skew);
  utimesSync(source, now, now);
  return { root, binary, sourceRoot, source };
}

describe('judgeBinaryFreshness — the stale-binary injection', () => {
  it('accepts a binary newer than its sources', () => {
    const tree = fakePluginTree();
    utimesSync(tree.binary, Date.now() / 1000 + 5, Date.now() / 1000 + 5);
    const verdict = judgeBinaryFreshness({ binary: tree.binary, sourceRoot: tree.sourceRoot });
    expect(verdict.fresh).toBe(true);
    expect(verdict.reason).toBe('FRESH');
    expect(verdict.binarySha256).toBe(sha256File(tree.binary));
  });

  it('REFUSES a binary older than the sources it was supposedly built from', () => {
    const tree = fakePluginTree({ binaryOlderBySeconds: 3600 });
    const verdict = judgeBinaryFreshness({ binary: tree.binary, sourceRoot: tree.sourceRoot });
    expect(verdict.fresh).toBe(false);
    expect(verdict.reason).toBe('STALE_BINARY');
    expect(verdict.newestInput).toBe(tree.source);
    expect(Number(verdict.staleByMs)).toBeGreaterThan(3_000_000);
  });

  it('REFUSES a binary that is not there at all, rather than reading it as fresh', () => {
    const tree = fakePluginTree();
    const verdict = judgeBinaryFreshness({ binary: join(tree.root, 'nope.so'), sourceRoot: tree.sourceRoot });
    expect(verdict.fresh).toBe(false);
    expect(verdict.reason).toBe('MISSING_BINARY');
  });
});

describe('parseAutomationLog — the truncated-run injection', () => {
  const line = (name: string, result?: string) => (result === undefined
    ? `LogAutomationController: Display: Test Started. Name={${name}} Path={${name}}`
    : `LogAutomationController: Display: Test Completed. Result={${result}} Name={${name}} Path={${name}}`);

  it('reads a complete green run as complete and green', () => {
    const text = [
      line('McpAutomationBridge.Foundation.IdempotencyLedger.Replay'),
      line('McpAutomationBridge.Foundation.IdempotencyLedger.Replay', 'Passed'),
      line('McpAutomationBridge.Core.RequestQueue.Fairness.PendingCaps'),
      line('McpAutomationBridge.Core.RequestQueue.Fairness.PendingCaps', 'Passed'),
      'LogAutomationController: Display: Tests Completed In: 4.2s',
    ].join('\n');
    const parsed = parseAutomationLog(text);
    expect(parsed.startedCount).toBe(2);
    expect(parsed.completedCount).toBe(2);
    expect(parsed.startedEqualsCompleted).toBe(true);
    expect(parsed.failed).toEqual([]);
    expect(parsed.tally).toEqual({ Passed: 2 });
    expect(parsed.sawQueueEmpty).toBe(true);
  });

  it('catches a run that started many and completed few', () => {
    const started = Array.from({ length: 214 }, (_, index) => line(`Suite.Test${index}`));
    const completed = Array.from({ length: 4 }, (_, index) => line(`Suite.Test${index}`, 'Passed'));
    const parsed = parseAutomationLog([...started, ...completed].join('\n'));
    expect(parsed.startedCount).toBe(214);
    expect(parsed.completedCount).toBe(4);
    // Nothing else in the log distinguishes this from a fast, green run.
    expect(parsed.failed).toEqual([]);
    expect(parsed.startedEqualsCompleted).toBe(false);
  });

  it('accepts Result={Success}, which is what Unreal actually writes', () => {
    // "Passed" is the automation UI's word; the log writes "Success".
    const parsed = parseAutomationLog([
      line('McpAutomationBridge.Core.Lifecycle.Drain.CancelScope'),
      line('McpAutomationBridge.Core.Lifecycle.Drain.CancelScope', 'Success'),
    ].join('\n'));
    expect(parsed.tally).toEqual({ Success: 1 });
    expect(parsed.failed).toEqual([]);
    expect(parsed.startedEqualsCompleted).toBe(true);
  });

  it('still treats a genuine non-pass result as a failure, whichever vocabulary is used', () => {
    const parsed = parseAutomationLog([
      line('A'), line('A', 'Success'),
      line('B'), line('B', 'Fail'),
      line('C'), line('C', 'NotRun'),
    ].join('\n'));
    expect(parsed.failed).toEqual(['B', 'C']);
  });

  it('does not read an empty log as a passing run', () => {
    const parsed = parseAutomationLog('LogInit: Display: Engine initialized\nLogExit: Exiting.');
    expect(parsed.startedEqualsCompleted).toBe(false);
    expect(parsed.completedCount).toBe(0);
  });

  it('surfaces the names of tests that completed with a non-Passed result', () => {
    const parsed = parseAutomationLog([
      line('A'), line('A', 'Passed'),
      line('B'), line('B', 'Failed'),
    ].join('\n'));
    expect(parsed.startedEqualsCompleted).toBe(true);
    expect(parsed.failed).toEqual(['B']);
    expect(parsed.tally).toEqual({ Passed: 1, Failed: 1 });
  });
});

describe('the wrong-root injection', () => {
  const io = {
    readFile: (path: string) => {
      if (path.endsWith('Engine/Build/Build.version')) {
        return JSON.stringify({ MajorVersion: 5, MinorVersion: 3, PatchVersion: 2, BranchName: 'UE5' });
      }
      if (path.endsWith('Version.h')) {
        return '#define ENGINE_MAJOR_VERSION\t5\n#define ENGINE_MINOR_VERSION\t3\n#define ENGINE_PATCH_VERSION\t2\n';
      }
      return '#!/bin/sh\n';
    },
    exists: (path: string) => !path.includes('UnrealEditor-Cmd'),
    isExecutable: () => false,
    describe: () => '5.3.2-release',
  };

  it('refuses a minor nothing on this machine reports', () => {
    const inventory = buildEngineInventory({ roots: ['/engines/UnrealEngine-5.7.0'], io });
    const verdict = inventory.resolve('5.7');
    expect(verdict.ok).toBe(false);
    expect(verdict.reason).toBe('MINOR_NOT_INSTALLED');
    expect(verdict.root).toBeNull();
  });

  it('files the mislabelled root under 5.3 and refuses it for lacking an editor', () => {
    const inventory = buildEngineInventory({ roots: ['/engines/UnrealEngine-5.7.0'], io });
    expect(inventory.available.map((entry) => entry.minorKey)).toEqual(['5.3']);
    const verdict = inventory.resolve('5.3');
    expect(verdict.ok).toBe(false);
    expect(verdict.reason).toBe('NO_COMPILED_EDITOR');
  });
});

describe('portAnswers', () => {
  it('reports nothing on a port nobody is listening on', async () => {
    expect(await portAnswers(65_527, 500)).toBe(false);
  });
});
