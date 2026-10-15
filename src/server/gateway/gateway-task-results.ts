// Calls that answered "still running" and their final replies, read back by manage_tools get_task_result: a client
// that gave up at its own timeout, or lost the idempotency key, had no way to the result but the log. Mirrors the
// native transport's finished-task store (RecordFinishedTask / DescribeTask in McpNativeTransportDynamicTools.cpp).

import { compactGatewayReply } from '../../utils/responses/gateway-reply-compaction.js';

/** The most recent calls kept; the oldest is dropped first. */
export const MAX_KEPT_TASKS = 32;

type TaskEntry = { readonly state: 'running' } | { readonly state: 'done'; readonly reply: Record<string, unknown> };

const tasks = new Map<string, TaskEntry>();

function remember(taskId: string, entry: TaskEntry): void {
  tasks.delete(taskId);
  tasks.set(taskId, entry);
  while (tasks.size > MAX_KEPT_TASKS) {
    const oldest = tasks.keys().next().value;
    if (oldest === undefined) break;
    tasks.delete(oldest);
  }
}

/** The call answered "still running" as taskId. */
export function noteTaskRunning(taskId: string): void {
  remember(taskId, { state: 'running' });
}

/** The call answered "still running" as taskId has finished with this reply. */
export function noteTaskDone(taskId: string, reply: Record<string, unknown>): void {
  remember(taskId, { state: 'done', reply });
}

/** What manage_tools get_task_result answers: the state, and once done the call's own outcome. */
export function describeTask(taskId: string): Record<string, unknown> {
  const entry = tasks.get(taskId);
  if (entry === undefined) {
    return {
      success: false,
      errorCode: 'TASK_NOT_FOUND',
      error: `No task ${taskId}: only a call answered "still running" since the server started is kept, the last ${MAX_KEPT_TASKS} of them.`
    };
  }
  if (entry.state === 'running') {
    return { success: true, taskId, state: 'running', message: `Task ${taskId} is still running.` };
  }
  // The call's own reply as it would have answered, compacted like every reply (data without the repeats).
  const outcome: Record<string, unknown> = { ...(compactGatewayReply(entry.reply) as Record<string, unknown>) };
  delete outcome.operation;
  outcome.success = entry.reply.success === true;
  return { success: true, taskId, state: 'done', outcome, message: `Task ${taskId} is done.` };
}

/** Test seam: forget every task. */
export function resetTasksForTest(): void {
  tasks.clear();
}
