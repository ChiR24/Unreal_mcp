// The same failing call, sent again unchanged, fails the same way, and agents (small models most) retried one call
// in a loop. After LOOP_LIMIT identical failures in a row within LOOP_WINDOW_MS, the next identical call is refused
// before it runs and pointed at the capability's contract. A success, another call's failure, a pause or a failure
// that names a passing moment ends the streak. McpNativeLoopGuard in the plugin keeps the same rule.

export const REPEATED_FAILURE_CODE = 'REPEATED_FAILURE';
export const LOOP_LIMIT = 3;
export const LOOP_WINDOW_MS = 60_000;

// A dialog, a busy or blocked editor, a timeout, a stale revision pin, consent, a quota or a cancel: the same call can
// succeed later, or with options or consent changed (which the params fingerprint does not see), so these never count.
const MOMENT_CODE = /^(?!INVALID_)\w*TIMEOUT$|_BUSY$|^CONSENT_|^(?:EDITOR_MODAL_ACTIVE|EDITOR_BLOCKED|EDITOR_TOOL_BLOCKED|STALE_STATE|QUOTA_EXCEEDED|NOT_CONNECTED|CANCELLED|MCP_REQUEST_CANCELLED)$/u;

interface Streak {
  readonly fingerprint: string;
  readonly count: number;
  readonly code: string;
  readonly at: number;
}

// ponytail: one streak per principal with no eviction; a process serves a handful of principals (local plus the
// scoped profiles a plugin advertises), so the map stays tiny.
const streaks = new Map<string, Streak>();

/** The streak this exact call would extend past the limit, or undefined when it may run. */
export function loopStreak(principal: string, fingerprint: string, now = Date.now()): Streak | undefined {
  const streak = streaks.get(principal);
  return streak?.fingerprint === fingerprint && streak.count >= LOOP_LIMIT && now - streak.at < LOOP_WINDOW_MS
    ? streak
    : undefined;
}

/** Records a dispatched call's receipt: a failure extends or starts the streak, anything else ends it. */
export function noteOutcome(principal: string, fingerprint: string, receipt: Record<string, unknown>, now = Date.now()): void {
  const code = typeof receipt.errorCode === 'string' ? receipt.errorCode : 'UNREAL_EXECUTION_ERROR';
  if (receipt.success === true || MOMENT_CODE.test(code)) {
    streaks.delete(principal);
    return;
  }
  const previous = streaks.get(principal);
  const repeat = previous !== undefined && previous.fingerprint === fingerprint && now - previous.at < LOOP_WINDOW_MS;
  streaks.set(principal, { fingerprint, count: repeat ? previous.count + 1 : 1, code, at: now });
}

export function repeatedFailureMessage(streak: Streak): string {
  return `This exact call failed ${String(streak.count)} times in a row (${streak.code}) and was not run again: sent unchanged it fails the same way. `
    + 'Change its params as that error said, run another call first (a read of what it named), or read the contract with nextCall.';
}

/** Test seam: forget every streak. */
export function resetLoopGuard(): void {
  streaks.clear();
}
