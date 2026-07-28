// src/server/mcp-primitives/task-checkpoint.ts
// Task 44: what a task-augmented `tools/call` is allowed to be on this server.
//
// MCP Tasks here are SAFE READ-ONLY CHECKPOINTS. A `search` or `describe` runs,
// and its result is retained under a pollable handle with a bounded TTL. The
// mutating operations are deliberately NOT offered as tasks: cancelling a task
// cannot interrupt work already dispatched to the Unreal editor (cancellation is
// advisory — the editor runs the request to completion), so handing back a
// cancellable handle for a mutation would promise an interruption this server
// cannot deliver. The refusal is raised BEFORE any work runs, and it is
// executable: it names the exact call that does work.
//
// Native mirror: Private/MCP/Primitives/McpTaskMethods.{h,cpp}. Both surfaces
// must accept the same operations and refuse with the same code.

import { ErrorCode, McpError, type Result, type Task } from '@modelcontextprotocol/sdk/types.js';
import type { RequestTaskStore } from '@modelcontextprotocol/sdk/shared/protocol.js';
import type { CreateTaskOptions } from '@modelcontextprotocol/sdk/experimental/tasks/interfaces.js';

/** The gateway operations that may be task-augmented. Read-only, in stable order. */
export const TASK_CHECKPOINT_OPERATIONS = ['search', 'describe'] as const;

export const TASK_CHECKPOINT_REFUSED = 'TASK_CHECKPOINT_NOT_AVAILABLE';

const CHECKPOINTABLE = new Set<string>(TASK_CHECKPOINT_OPERATIONS);

/**
 * The reason this task-augmented call cannot be honoured, or `undefined` when it
 * can. Never runs anything: a call that is about to be refused must not have
 * already mutated the editor.
 */
export function taskCheckpointRefusal(
  toolName: string,
  args: Record<string, unknown>,
  taskStore: RequestTaskStore | undefined,
): McpError | undefined {
  if (taskStore === undefined) {
    // Unreachable while server-factory advertises `tasks` only alongside the
    // store, which is exactly the invariant this arm exists to keep loud.
    return new McpError(
      ErrorCode.InternalError,
      'Task creation was accepted at the capability level but this server has no task store configured.',
    );
  }
  if (toolName !== 'unreal') {
    return new McpError(
      ErrorCode.InvalidParams,
      `Task creation is not available for '${toolName}'. The only public tool is the 'unreal' gateway.`,
      { code: TASK_CHECKPOINT_REFUSED, nextCall: { method: 'tools/call', params: { name: 'unreal' } } },
    );
  }
  const operation = typeof args.operation === 'string' ? args.operation : '';
  if (!CHECKPOINTABLE.has(operation)) {
    return new McpError(
      ErrorCode.InvalidParams,
      `MCP Tasks on this server are safe read-only checkpoints, so only ${TASK_CHECKPOINT_OPERATIONS.join(' and ')} ` +
        `may be task-augmented; '${operation || 'unknown'}' may not. Cancelling a task cannot interrupt work already ` +
        'dispatched to the editor, so a mutating operation is never handed back as a cancellable handle. ' +
        'Re-send this call without params.task to run it synchronously.',
      {
        code: TASK_CHECKPOINT_REFUSED,
        taskableOperations: [...TASK_CHECKPOINT_OPERATIONS],
        nextCall: { method: 'tools/call', params: { name: 'unreal', arguments: { operation } } },
      },
    );
  }
  return undefined;
}

function creationOptions(requested: CreateTaskOptions): CreateTaskOptions {
  return {
    ...(requested.ttl === undefined ? {} : { ttl: requested.ttl }),
    ...(typeof requested.pollInterval === 'number' ? { pollInterval: requested.pollInterval } : {}),
  };
}

/**
 * Run one read-only checkpoint and hand back its task. The stored result is the
 * SAME payload the synchronous path would have returned, so `tasks/result` and a
 * plain `tools/call` cannot disagree about what happened.
 */
export async function runTaskCheckpoint(input: {
  readonly taskStore: RequestTaskStore;
  readonly taskCreation: CreateTaskOptions;
  readonly run: () => Promise<Result>;
}): Promise<{ task: Task }> {
  const created = await input.taskStore.createTask(creationOptions(input.taskCreation));

  let status: 'completed' | 'failed' = 'completed';
  let result: Result;
  try {
    result = await input.run();
    if (result.isError === true) status = 'failed';
  } catch (error) {
    status = 'failed';
    result = { isError: true, message: error instanceof Error ? error.message : String(error) };
  }

  try {
    await input.taskStore.storeTaskResult(created.taskId, status, result);
  } catch {
    // The task reached a terminal state while this one ran — a cancellation, or
    // a duplicate settle. The published terminal state wins and this late result
    // is dropped rather than overwriting it. The handle below then reports the
    // state the client was already told about.
  }

  return { task: await input.taskStore.getTask(created.taskId) };
}
