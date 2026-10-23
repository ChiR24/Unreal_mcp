// The loop guard: the same failing call is refused after LOOP_LIMIT failures in a row, and only then.

import { beforeEach, describe, expect, it } from 'vitest';

import { LOOP_LIMIT, LOOP_WINDOW_MS, loopStreak, noteOutcome, resetLoopGuard } from './gateway-loop-guard.js';

const failed = (errorCode = 'ACTOR_NOT_FOUND'): Record<string, unknown> => ({ success: false, errorCode });
const failTimes = (count: number, fingerprint = 'f1', code?: string, at = 0): void => {
  for (let i = 0; i < count; i += 1) noteOutcome('local', fingerprint, failed(code), at);
};

describe('loop guard', () => {
  beforeEach(resetLoopGuard);

  it('lets a call run until it has failed LOOP_LIMIT times in a row, then stops it', () => {
    failTimes(LOOP_LIMIT - 1);
    expect(loopStreak('local', 'f1', 0)).toBeUndefined();
    failTimes(1);
    expect(loopStreak('local', 'f1', 0)).toMatchObject({ count: LOOP_LIMIT, code: 'ACTOR_NOT_FOUND' });
  });

  it('stops only the identical call and only for its principal', () => {
    failTimes(LOOP_LIMIT);
    expect(loopStreak('local', 'f2', 0)).toBeUndefined();
    expect(loopStreak('scoped:other', 'f1', 0)).toBeUndefined();
  });

  it('ends the streak on a success, another call failing, or a pause', () => {
    failTimes(LOOP_LIMIT);
    noteOutcome('local', 'f9', { success: true }, 0);
    expect(loopStreak('local', 'f1', 0)).toBeUndefined();

    failTimes(LOOP_LIMIT - 1);
    failTimes(1, 'f2');
    failTimes(1);
    expect(loopStreak('local', 'f1', 0)).toBeUndefined();

    failTimes(LOOP_LIMIT);
    expect(loopStreak('local', 'f1', LOOP_WINDOW_MS)).toBeUndefined();
    failTimes(LOOP_LIMIT - 1, 'f3', undefined, 0);
    failTimes(1, 'f3', undefined, LOOP_WINDOW_MS);
    expect(loopStreak('local', 'f3', LOOP_WINDOW_MS)).toBeUndefined();
  });

  it('never counts a failure that names a passing moment', () => {
    for (const code of ['EDITOR_MODAL_ACTIVE', 'EDITOR_BLOCKED', 'TOOL_TIMEOUT', 'PIE_START_TIMEOUT', 'AUTOMATION_BUSY',
      'STALE_STATE', 'CONSENT_REQUIRED', 'QUOTA_EXCEEDED', 'MCP_REQUEST_CANCELLED']) {
      failTimes(LOOP_LIMIT, 'f1', code);
      expect(loopStreak('local', 'f1', 0), code).toBeUndefined();
    }
    failTimes(LOOP_LIMIT, 'f1', 'INVALID_TIMEOUT');
    expect(loopStreak('local', 'f1', 0)).toBeDefined();
  });
});
