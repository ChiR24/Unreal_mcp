import type { ITools } from '../../types/tools/tool-interfaces.js';
import { Logger } from '../../utils/logging/logger.js';
import { ResponseFactory } from '../../utils/responses/response-factory.js';
import {
  addErrorContext,
  classifyExecutionError,
  mergeActionParams,
  normalizeToolCall
} from './consolidated-call-utils.js';
import { normalizeAutomationFrame } from './automation-frame-normalization.js';
import { toolRegistry } from './dynamic-handler-registry.js';

function hasStringMessage(value: unknown): value is { readonly message: string } {
  return value !== null
    && typeof value === 'object'
    && 'message' in value
    && typeof value.message === 'string';
}

export async function handleConsolidatedToolCall(
  name: string,
  args: Record<string, unknown>,
  tools: ITools
) {
  const logger = new Logger('ConsolidatedToolHandler');
  const startTime = Date.now();
  let actionForError: string | undefined;

  try {
    const expandedArgs = mergeActionParams(args);
    const normalized = normalizeToolCall(name, expandedArgs);
    const normalizedArgs = normalized.args;
    actionForError = normalized.action;

    if (normalized.action && !normalizedArgs.action) {
      normalizedArgs.action = normalized.action;
    }

    const handler = toolRegistry.getHandler(normalized.name);
    if (handler) {
      const result = normalizeAutomationFrame(await handler(normalizedArgs, tools));
      return addErrorContext(result, normalized.name, actionForError);
    }

    return ResponseFactory.errorWithCode('UNKNOWN_TOOL', `Unknown consolidated tool: ${name}`, {
      toolName: name,
      action: actionForError ?? null
    });
  } catch (err: unknown) {
    const duration = Date.now() - startTime;
    const errorMessage = err instanceof Error || hasStringMessage(err) ? err.message : String(err);
    logger.error(`Failed execution of ${name} after ${duration}ms: ${errorMessage}`);

    const errorCode = classifyExecutionError(errorMessage);
    const isTimeout = errorCode === 'TOOL_TIMEOUT';
    const text = isTimeout
      ? `Tool ${name} timed out. Please check Unreal Engine connection.`
      : `Failed to execute ${name}: ${errorMessage}`;

    return ResponseFactory.errorWithCode(errorCode, text, {
      toolName: name,
      action: actionForError ?? null,
      durationMs: duration
    });
  }
}
