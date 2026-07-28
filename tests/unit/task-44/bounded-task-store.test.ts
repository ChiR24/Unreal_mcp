// Task 44 — the per-session BOUNDED task store, proven against a fake clock.
//
// Every temporal assertion here advances an injected clock; there is no
// setTimeout, no sleep and no real elapsed time anywhere in this file, so an
// expiry case cannot pass by being slow or fail by being fast.
//
// The four properties that make this store safe to reach from the wire:
//   1. a hard per-session cap that is never exceeded,
//   2. deterministic eviction that only ever removes a TERMINAL task, so a live
//      task can never vanish from under a polling client,
//   3. TTL expiry measured from creation, regardless of status,
//   4. session isolation — one session cannot read, cancel, or evict another's
//      task, and cannot even learn that it exists.
// Plus the terminal-state invariant: exactly one terminal transition per task,
// so a late or duplicate result can never overwrite the one already published.

import { describe, expect, it } from 'vitest';
import {
    BoundedTaskStore,
    TaskStoreCapacityError,
    TaskStoreNotFoundError,
    TaskStoreTerminalError,
} from '../../../src/server/mcp-primitives/bounded-task-store.js';

// A fake clock: the ONLY source of time in this suite.
function fakeClock(start = 1_000_000): { now: () => number; advance: (ms: number) => void } {
    let current = start;
    return {
        now: () => current,
        advance: (ms: number) => {
            current += ms;
        },
    };
}

const REQUEST = { method: 'tools/call', params: { name: 'unreal' } } as const;

function store(
    clock: { now: () => number },
    overrides: Partial<{ maxTasksPerSession: number; defaultTtlMs: number; maxTtlMs: number }> = {},
): BoundedTaskStore {
    return new BoundedTaskStore({
        now: clock.now,
        maxTasksPerSession: overrides.maxTasksPerSession ?? 4,
        defaultTtlMs: overrides.defaultTtlMs ?? 60_000,
        maxTtlMs: overrides.maxTtlMs ?? 300_000,
    });
}

async function createFinished(subject: BoundedTaskStore, session: string, seq: number): Promise<string> {
    const task = await subject.createTask({}, seq, REQUEST, session);
    await subject.storeTaskResult(task.taskId, 'completed', { content: [] }, session);
    return task.taskId;
}

describe('Task 44 — BoundedTaskStore capacity', () => {
    it('never exceeds the declared per-session cap', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 3 });

        const ids: string[] = [];
        for (let i = 0; i < 10; i += 1) {
            ids.push(await createFinished(subject, 'session-a', i));
        }

        expect(subject.sessionSize('session-a')).toBe(3);
        const listed = await subject.listTasks(undefined, 'session-a');
        expect(listed.tasks).toHaveLength(3);
    });

    it('evicts the OLDEST terminal task first, in insertion order', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 3 });

        const first = await createFinished(subject, 'session-a', 1);
        const second = await createFinished(subject, 'session-a', 2);
        const third = await createFinished(subject, 'session-a', 3);
        // Creating a fourth must displace exactly `first`.
        const fourth = await createFinished(subject, 'session-a', 4);

        expect(await subject.getTask(first, 'session-a')).toBeNull();
        expect(await subject.getTask(second, 'session-a')).not.toBeNull();
        expect(await subject.getTask(third, 'session-a')).not.toBeNull();
        expect(await subject.getTask(fourth, 'session-a')).not.toBeNull();

        // A fifth displaces `second`, proving the order is insertion order and
        // not "whatever the map iterator happened to yield".
        const fifth = await createFinished(subject, 'session-a', 5);
        expect(await subject.getTask(second, 'session-a')).toBeNull();
        expect(await subject.getTask(third, 'session-a')).not.toBeNull();
        expect(await subject.getTask(fifth, 'session-a')).not.toBeNull();
    });

    it('REFUSES creation rather than evicting a live task', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 2 });

        // Two working tasks: nothing terminal is available to evict.
        const live = await subject.createTask({}, 1, REQUEST, 'session-a');
        await subject.createTask({}, 2, REQUEST, 'session-a');

        await expect(subject.createTask({}, 3, REQUEST, 'session-a')).rejects.toBeInstanceOf(
            TaskStoreCapacityError,
        );
        // The live task is untouched — a polling client keeps its handle.
        expect(await subject.getTask(live.taskId, 'session-a')).not.toBeNull();
        expect(subject.sessionSize('session-a')).toBe(2);
    });

    it('prefers evicting a terminal task over refusing, even when live tasks are older', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 3 });

        const liveOld = await subject.createTask({}, 1, REQUEST, 'session-a');
        const done = await createFinished(subject, 'session-a', 2);
        const liveNew = await subject.createTask({}, 3, REQUEST, 'session-a');

        const fresh = await subject.createTask({}, 4, REQUEST, 'session-a');

        expect(await subject.getTask(done, 'session-a')).toBeNull();
        expect(await subject.getTask(liveOld.taskId, 'session-a')).not.toBeNull();
        expect(await subject.getTask(liveNew.taskId, 'session-a')).not.toBeNull();
        expect(await subject.getTask(fresh.taskId, 'session-a')).not.toBeNull();
    });
});

describe('Task 44 — BoundedTaskStore TTL expiry (fake clock only)', () => {
    it('expires a task once its TTL has elapsed, regardless of status', async () => {
        const clock = fakeClock();
        const subject = store(clock, { defaultTtlMs: 10_000 });

        const finished = await createFinished(subject, 'session-a', 1);
        const working = await subject.createTask({}, 2, REQUEST, 'session-a');

        clock.advance(9_999);
        expect(await subject.getTask(finished, 'session-a')).not.toBeNull();
        expect(await subject.getTask(working.taskId, 'session-a')).not.toBeNull();

        clock.advance(1);
        expect(await subject.getTask(finished, 'session-a')).toBeNull();
        expect(await subject.getTask(working.taskId, 'session-a')).toBeNull();
        expect(subject.sessionSize('session-a')).toBe(0);
        expect((await subject.listTasks(undefined, 'session-a')).tasks).toHaveLength(0);
    });

    it('expiry frees capacity without ever evicting a live task', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 2, defaultTtlMs: 10_000 });

        await subject.createTask({}, 1, REQUEST, 'session-a');
        await subject.createTask({}, 2, REQUEST, 'session-a');
        await expect(subject.createTask({}, 3, REQUEST, 'session-a')).rejects.toBeInstanceOf(
            TaskStoreCapacityError,
        );

        clock.advance(10_000);
        const afterExpiry = await subject.createTask({}, 4, REQUEST, 'session-a');
        expect(afterExpiry.taskId).toBeTruthy();
        expect(subject.sessionSize('session-a')).toBe(1);
    });

    it('clamps a requested TTL to the ceiling and reports the TTL it actually applied', async () => {
        const clock = fakeClock();
        const subject = store(clock, { defaultTtlMs: 60_000, maxTtlMs: 120_000 });

        const clamped = await subject.createTask({ ttl: 10 * 60_000 }, 1, REQUEST, 'session-a');
        expect(clamped.ttl).toBe(120_000);

        const honoured = await subject.createTask({ ttl: 30_000 }, 2, REQUEST, 'session-a');
        expect(honoured.ttl).toBe(30_000);

        const defaulted = await subject.createTask({}, 3, REQUEST, 'session-a');
        expect(defaulted.ttl).toBe(60_000);
    });

    it('never grants the unlimited lifetime a null TTL asks for', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTtlMs: 120_000 });

        const task = await subject.createTask({ ttl: null }, 1, REQUEST, 'session-a');
        expect(task.ttl).toBe(120_000);

        clock.advance(120_000);
        expect(await subject.getTask(task.taskId, 'session-a')).toBeNull();
    });
});

describe('Task 44 — BoundedTaskStore session isolation', () => {
    it('does not let one session READ another session task', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const owned = await createFinished(subject, 'session-a', 1);

        expect(await subject.getTask(owned, 'session-b')).toBeNull();
        await expect(subject.getTaskResult(owned, 'session-b')).rejects.toBeInstanceOf(
            TaskStoreNotFoundError,
        );
        expect((await subject.listTasks(undefined, 'session-b')).tasks).toHaveLength(0);
        // The owner is unaffected.
        expect(await subject.getTask(owned, 'session-a')).not.toBeNull();
    });

    it('does not let one session CANCEL another session task', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');

        await expect(
            subject.updateTaskStatus(created.taskId, 'cancelled', 'not yours', 'session-b'),
        ).rejects.toBeInstanceOf(TaskStoreNotFoundError);

        const still = await subject.getTask(created.taskId, 'session-a');
        expect(still?.status).toBe('working');
    });

    it('does not let one session EVICT another session task', async () => {
        const clock = fakeClock();
        const subject = store(clock, { maxTasksPerSession: 2 });
        const a1 = await createFinished(subject, 'session-a', 1);
        const a2 = await createFinished(subject, 'session-a', 2);

        // Session B fills and overflows its own partition repeatedly.
        for (let i = 0; i < 6; i += 1) await createFinished(subject, 'session-b', i);

        expect(await subject.getTask(a1, 'session-a')).not.toBeNull();
        expect(await subject.getTask(a2, 'session-a')).not.toBeNull();
        expect(subject.sessionSize('session-a')).toBe(2);
        expect(subject.sessionSize('session-b')).toBe(2);
    });

    it('does not let one session WRITE a result into another session task', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');

        await expect(
            subject.storeTaskResult(created.taskId, 'completed', { hijacked: true }, 'session-b'),
        ).rejects.toBeInstanceOf(TaskStoreNotFoundError);
        expect((await subject.getTask(created.taskId, 'session-a'))?.status).toBe('working');
    });

    it('keeps the undefined-session partition separate from every named session', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const anonymous = await createFinished(subject, undefined as unknown as string, 1);

        expect(await subject.getTask(anonymous, 'session-a')).toBeNull();
        expect(await subject.getTask(anonymous, undefined)).not.toBeNull();
    });
});

describe('Task 44 — BoundedTaskStore terminal-state invariant', () => {
    it('accepts exactly ONE terminal transition per task', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');

        await subject.storeTaskResult(created.taskId, 'completed', { first: true }, 'session-a');
        await expect(
            subject.storeTaskResult(created.taskId, 'completed', { second: true }, 'session-a'),
        ).rejects.toBeInstanceOf(TaskStoreTerminalError);
        await expect(
            subject.storeTaskResult(created.taskId, 'failed', { third: true }, 'session-a'),
        ).rejects.toBeInstanceOf(TaskStoreTerminalError);

        expect(await subject.getTaskResult(created.taskId, 'session-a')).toEqual({ first: true });
    });

    it('refuses a LATE result after cancellation, so a cancelled task never reads completed', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');

        await subject.updateTaskStatus(created.taskId, 'cancelled', 'client cancelled', 'session-a');
        await expect(
            subject.storeTaskResult(created.taskId, 'completed', { late: true }, 'session-a'),
        ).rejects.toBeInstanceOf(TaskStoreTerminalError);

        expect((await subject.getTask(created.taskId, 'session-a'))?.status).toBe('cancelled');
    });

    it('refuses any status transition out of a terminal state', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');
        await subject.storeTaskResult(created.taskId, 'completed', {}, 'session-a');

        await expect(
            subject.updateTaskStatus(created.taskId, 'working', undefined, 'session-a'),
        ).rejects.toBeInstanceOf(TaskStoreTerminalError);
        await expect(
            subject.updateTaskStatus(created.taskId, 'cancelled', undefined, 'session-a'),
        ).rejects.toBeInstanceOf(TaskStoreTerminalError);
    });

    it('has no result to hand out before the task reaches a terminal state', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');

        await expect(subject.getTaskResult(created.taskId, 'session-a')).rejects.toBeInstanceOf(
            TaskStoreNotFoundError,
        );
    });

    it('advances lastUpdatedAt on the fake clock when a task settles', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const created = await subject.createTask({}, 1, REQUEST, 'session-a');
        expect(created.createdAt).toBe(created.lastUpdatedAt);

        clock.advance(5_000);
        await subject.storeTaskResult(created.taskId, 'completed', {}, 'session-a');
        const settled = await subject.getTask(created.taskId, 'session-a');
        expect(new Date(String(settled?.lastUpdatedAt)).getTime()).toBe(
            new Date(created.createdAt).getTime() + 5_000,
        );
    });
});

describe('Task 44 — BoundedTaskStore cleanup', () => {
    it('closeSession drops every task and marker for that session only', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const owned = await createFinished(subject, 'session-a', 1);
        const other = await createFinished(subject, 'session-b', 2);

        subject.closeSession('session-a');

        expect(await subject.getTask(owned, 'session-a')).toBeNull();
        expect(subject.sessionSize('session-a')).toBe(0);
        expect(subject.totalSize()).toBe(1);
        expect(await subject.getTask(other, 'session-b')).not.toBeNull();
    });

    it('clear drops everything across every session', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        await createFinished(subject, 'session-a', 1);
        await createFinished(subject, 'session-b', 2);

        subject.clear();

        expect(subject.totalSize()).toBe(0);
        expect((await subject.listTasks(undefined, 'session-a')).tasks).toHaveLength(0);
        expect((await subject.listTasks(undefined, 'session-b')).tasks).toHaveLength(0);
    });

    it('leaves no retained result behind after cleanup', async () => {
        const clock = fakeClock();
        const subject = store(clock);
        const owned = await createFinished(subject, 'session-a', 1);

        subject.clear();

        await expect(subject.getTaskResult(owned, 'session-a')).rejects.toBeInstanceOf(
            TaskStoreNotFoundError,
        );
    });
});
