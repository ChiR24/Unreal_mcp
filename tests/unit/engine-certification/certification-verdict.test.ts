// tests/unit/engine-certification/certification-verdict.test.ts
// Task 52 — the failure injections the plan names that no return code reports.
//
// certification-stages.test.ts already injects the wrong root, the stale binary
// and the truncated automation run; disposable-project.test.ts takes a real port
// with a real listener. What is left are the three that a green-looking run can
// carry all the way into evidence:
//
//   the editor CRASH, told apart from a hang and from a port that answers while
//   this run's editor is gone — the failure that looks most like success;
//   the CLEANUP MISMATCH, where the teardown grades itself and is wrong;
//   the TREE that moved under a ninety-minute run on a shared worktree.
//
// All of it runs offline against a throwaway /proc and injected readings, so a
// regression is caught on the commit that causes it rather than during the next
// engine certification.

import { describe, expect, it } from 'vitest';
import { mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import {
  CLEANUP_AGREEMENT,
  EDITOR_LIVENESS,
  extractCrashSignature,
  judgeCleanupAgreement,
  judgeCleanupRelease,
  judgeEditorLiveness,
  judgeProcessRelease,
  judgeTreeStability,
} from './certification-verdict.mjs';

/** A throwaway /proc holding exactly the processes a case wants to exist. */
function fakeProc(entries: { pid: number; state?: string; cmdline?: string }[]) {
  const root = mkdtempSync(join(tmpdir(), 'task52-verdict-proc-'));
  for (const entry of entries) {
    const dir = join(root, String(entry.pid));
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, 'stat'), `${entry.pid} (UnrealEditor) ${entry.state ?? 'S'} 1 0 0 0 -1 0 0 0 0 0 0 0`);
    writeFileSync(join(dir, 'cmdline'), (entry.cmdline ?? '/engine/UnrealEditor-Cmd').split(' ').join('\0'));
  }
  return root;
}

const listener = (present: boolean | null, extra: Record<string, unknown> = {}) => ({
  present, mechanism: 'procfs:net-tcp', conclusive: present !== null, detail: { port: 41911 }, ...extra,
});

describe('extractCrashSignature', () => {
  it('finds the line Unreal writes when it dies', () => {
    const signature = extractCrashSignature([
      'LogInit: Display: Engine initialized',
      'LogWindows: Error: === Critical error: ===',
      'LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION',
    ].join('\n'));
    expect(signature.crashed).toBe(true);
    expect(signature.marker).toBe('CRITICAL_ERROR');
    expect(signature.excerpt).toContain('Critical error');
  });

  it('does NOT read an ensure as a crash', () => {
    // An ensure prints a full callstack and the editor carries on — the 5.7 run
    // on this lane printed one and then completed all 84 automation tests.
    const signature = extractCrashSignature([
      'LogOutputDevice: Warning: Ensure condition failed: Slot != INDEX_NONE [File:Engine.cpp] [Line: 1200]',
      'LogOutputDevice: Warning: Stack: 0x00007fff',
      'LogAutomationController: Display: Tests Completed In: 214.7s',
    ].join('\n'));
    expect(signature.crashed).toBe(false);
    expect(signature.marker).toBeNull();
  });

  it('reads nothing out of an empty or ordinary log', () => {
    expect(extractCrashSignature('').crashed).toBe(false);
    expect(extractCrashSignature('LogExit: Exiting.').crashed).toBe(false);
  });

  it('scans the tail, where a dying process prints its reason', () => {
    const padded = `${'LogTemp: Display: chatter\n'.repeat(20_000)}Fatal error: [File:Assert.cpp] [Line: 12] bad`;
    const signature = extractCrashSignature(padded);
    expect(signature.marker).toBe('FATAL_ERROR');
  });
});

describe('judgeEditorLiveness — the editor-crash injection', () => {
  it('passes an editor that is running and answering', () => {
    const procRoot = fakeProc([{ pid: 4242 }]);
    const verdict = judgeEditorLiveness({ pid: 4242, portReady: true, procRoot, logText: 'LogInit: Display: ok' });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.READY);
    expect(verdict.ok).toBe(true);
  });

  it('calls a crash a CRASH, and carries the marker into the detail', () => {
    const procRoot = fakeProc([]);
    const verdict = judgeEditorLiveness({
      pid: 4242, portReady: false, procRoot,
      logText: 'LogWindows: Error: Fatal error: [File:D:/build/Engine.cpp] [Line: 42] Array index out of bounds',
    });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.CRASHED);
    expect(verdict.ok).toBe(false);
    expect(verdict.crash.marker).toBe('FATAL_ERROR');
    expect(verdict.detail).toContain('Array index out of bounds');
  });

  it('still calls a silent exit a crash rather than inventing a reason for it', () => {
    const verdict = judgeEditorLiveness({ pid: 4242, portReady: false, procRoot: fakeProc([]), logText: 'LogExit: Exiting.' });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.CRASHED);
    expect(verdict.crash.crashed).toBe(false);
    expect(verdict.detail).toContain('names no crash marker');
  });

  it('separates a HANG from a crash, because cleanup still has an editor to end', () => {
    const procRoot = fakeProc([{ pid: 4242 }]);
    const verdict = judgeEditorLiveness({ pid: 4242, portReady: false, procRoot, logText: 'LogInit: Display: loading' });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.HUNG);
    expect(verdict.ok).toBe(false);
    expect(verdict.alive).toBe(true);
  });

  it('REFUSES a port that answers while this run\'s editor is gone', () => {
    // The failure that looks most like success: something is listening, every
    // driver would happily score against it, and it is not ours.
    const verdict = judgeEditorLiveness({ pid: 4242, portReady: true, procRoot: fakeProc([]), logText: '' });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.PORT_NOT_OURS);
    expect(verdict.ok).toBe(false);
    expect(verdict.detail).toContain('was not started here');
  });

  it('refuses a launch that never produced a pid', () => {
    const verdict = judgeEditorLiveness({ pid: null, portReady: false, logText: '' });
    expect(verdict.verdict).toBe(EDITOR_LIVENESS.NEVER_LAUNCHED);
    expect(verdict.observation).toBeNull();
  });

  it('treats a zombie as not-running, so a leak check cannot pass on an unreaped process', () => {
    const procRoot = fakeProc([{ pid: 4242, state: 'Z' }]);
    expect(judgeEditorLiveness({ pid: 4242, portReady: false, procRoot }).verdict).toBe(EDITOR_LIVENESS.CRASHED);
  });
});

describe('judgeCleanupRelease — the cleanup-mismatch injection', () => {
  it('accepts a release only when a DIFFERENT mechanism also finds it gone', () => {
    const row = judgeCleanupRelease({
      resource: '127.0.0.1:41911', claimedReleased: true, claimedBy: 'connect() probe', observation: listener(false),
    });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.AGREED_RELEASED);
    expect(row.ok).toBe(true);
  });

  it('catches the teardown that graded itself: claimed released, still there', () => {
    const row = judgeCleanupRelease({
      resource: '127.0.0.1:41911', claimedReleased: true, claimedBy: 'connect() probe', observation: listener(true),
    });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.DISAGREEMENT);
    expect(row.ok).toBe(false);
    expect(row.reason).toContain('still sees it');
  });

  it('records an honest leak as a leak, not as a disagreement', () => {
    const row = judgeCleanupRelease({
      resource: '/tmp/opencode/task52-x', claimedReleased: false, claimedBy: 'rm receipt',
      observation: { present: true, mechanism: 'fs:tree-walk', conclusive: true },
    });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.AGREED_LEAKED);
    expect(row.ok).toBe(false);
  });

  it('refuses to call an INCONCLUSIVE reading a release', () => {
    // /proc/net/tcp unreadable is exactly this: the oracle did not look, and a
    // clean bill of health derived from a failed look is worse than no reading.
    for (const seen of [listener(null, { detail: { reason: 'PROC_NET_TCP_UNREADABLE' } }), null]) {
      const row = judgeCleanupRelease({ resource: 'port', claimedReleased: true, claimedBy: 'connect() probe', observation: seen });
      expect(row.verdict).toBe(CLEANUP_AGREEMENT.INCONCLUSIVE);
      expect(row.ok).toBe(false);
      expect(row.reason).toContain('is not "it is gone"');
    }
  });

  it('refuses the benign-looking contradiction too, because neither reading can be reported', () => {
    const row = judgeCleanupRelease({
      resource: '/tmp/opencode/task52-x', claimedReleased: false, claimedBy: 'rm threw EBUSY',
      observation: { present: false, mechanism: 'fs:tree-walk', conclusive: true },
    });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.DISAGREEMENT);
    expect(row.ok).toBe(false);
  });
});

describe('judgeProcessRelease', () => {
  const sent: { pid: number; signal: number }[] = [];
  const kill = (pid: number, signal: number) => {
    sent.push({ pid, signal });
    if (pid === 4242) return; // alive
    const error = new Error('no such process') as Error & { code: string };
    error.code = 'ESRCH';
    throw error;
  };

  it('agrees a process is released only when the syscall and procfs both say so', () => {
    const row = judgeProcessRelease({ pid: 5150, procRoot: fakeProc([]), kill });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.AGREED_RELEASED);
    expect(row.ok).toBe(true);
  });

  it('reports a process that both mechanisms still see as a leak', () => {
    const row = judgeProcessRelease({ pid: 4242, procRoot: fakeProc([{ pid: 4242 }]), kill });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.AGREED_LEAKED);
    expect(row.ok).toBe(false);
  });

  it('does not fail a clean run on an unreaped zombie, and still says so', () => {
    const row = judgeProcessRelease({ pid: 4242, procRoot: fakeProc([{ pid: 4242, state: 'Z' }]), kill });
    expect(row.verdict).toBe(CLEANUP_AGREEMENT.REAPING);
    expect(row.ok).toBe(true);
    expect(row.reason).toContain('holds no port');
  });

  it('reads EPERM as alive — a pid this run spawned that now belongs to someone else is not gone', () => {
    const eperm = (_pid: number, _signal: number) => {
      const error = new Error('operation not permitted') as Error & { code: string };
      error.code = 'EPERM';
      throw error;
    };
    const row = judgeProcessRelease({ pid: 4242, procRoot: fakeProc([{ pid: 4242 }]), kill: eperm });
    expect(row.claimedReleased).toBe(false);
    expect(row.ok).toBe(false);
  });

  it('never signals anything but signal 0 — a judgement must not become an action', () => {
    sent.length = 0;
    judgeProcessRelease({ pid: 5150, procRoot: fakeProc([]), kill });
    judgeProcessRelease({ pid: 4242, procRoot: fakeProc([{ pid: 4242 }]), kill });
    expect(sent.length).toBe(2);
    expect(sent.every((entry) => entry.signal === 0)).toBe(true);
  });
});

describe('judgeCleanupAgreement', () => {
  const released = (resource: string) => judgeCleanupRelease({ resource, claimedReleased: true, claimedBy: 'probe', observation: listener(false) });

  it('passes only when every owned resource is agreed gone', () => {
    const verdict = judgeCleanupAgreement({ rows: [released('a'), released('b'), released('c')] });
    expect(verdict.ok).toBe(true);
    expect(verdict.checked).toBe(3);
    expect(verdict.detail).toContain('two independent readings agree');
  });

  it('fails the whole cleanup on one disagreement and names it', () => {
    const bad = judgeCleanupRelease({ resource: '127.0.0.1:41911', claimedReleased: true, claimedBy: 'probe', observation: listener(true) });
    const verdict = judgeCleanupAgreement({ rows: [released('a'), bad] });
    expect(verdict.ok).toBe(false);
    expect(verdict.disagreements).toHaveLength(1);
    expect(verdict.detail).toContain('127.0.0.1:41911');
  });
});

describe('judgeTreeStability — the shared-worktree injection', () => {
  const recorded = [
    { path: 'tests/unit/engine-certification/engine-inventory.mjs', sha256: 'aaa' },
    { path: 'scripts/qa/adversarial.mjs', sha256: 'bbb' },
  ];

  it('passes when every recorded file is byte-identical to what the run started from', () => {
    const verdict = judgeTreeStability({ recorded, projectRoot: '/repo', hash: (file) => (file.endsWith('engine-inventory.mjs') ? 'aaa' : 'bbb') });
    expect(verdict.stable).toBe(true);
    expect(verdict.checked).toBe(2);
  });

  it('names the file another lane changed mid-run, and the stage it changed by', () => {
    const verdict = judgeTreeStability({
      recorded, projectRoot: '/repo', stage: 'drivers',
      hash: (file) => (file.endsWith('engine-inventory.mjs') ? 'aaa' : 'REGENERATED'),
    });
    expect(verdict.stable).toBe(false);
    expect(verdict.moved.map((entry) => entry.path)).toEqual(['scripts/qa/adversarial.mjs']);
    expect(verdict.detail).toContain('by drivers');
    expect(verdict.detail).toContain('no longer certifies one tree');
  });

  it('treats a file that vanished as a change, not as an unchanged file', () => {
    const verdict = judgeTreeStability({ recorded, projectRoot: '/repo', hash: () => null });
    expect(verdict.stable).toBe(false);
    expect(verdict.moved).toHaveLength(2);
    expect(verdict.detail).toContain('now unreadable');
  });
});
