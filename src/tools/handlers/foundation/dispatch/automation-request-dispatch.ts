import { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { CommandValidator } from '../../../../utils/commands/command-validator.js';
import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import { validateArgsSecurity } from '../arguments/handler-argument-validation.js';
import { getTimeoutMs } from './handler-timeout.js';
import { normalizePathFields } from '../normalization/ue-path-normalization.js';

export interface SubActionDispatcher {
  argsRecord: Record<string, unknown>;
  sendRequest(subAction: string, extraPayload?: Record<string, unknown>): Promise<Record<string, unknown>>;
}

function requireConsoleCommandString(value: unknown, context: string): string {
  if (typeof value !== 'string') {
    throw new Error(`${context} must be a string`);
  }
  return value;
}

function validateConsoleCommandPayload(toolName: string, args: Record<string, unknown>): void {
  const normalizedToolName = toolName.toLowerCase();
  if (normalizedToolName === 'console_command') {
    CommandValidator.validate(requireConsoleCommandString(args.command, 'console_command.command'));
    return;
  }

  if (normalizedToolName !== 'batch_console_commands') {
    return;
  }

  const commands = args.commands;
  if (!Array.isArray(commands)) {
    throw new Error('batch_console_commands.commands must be an array');
  }

  for (const [index, entry] of commands.entries()) {
    if (typeof entry === 'string') {
      CommandValidator.validate(entry);
      continue;
    }

    if (entry && typeof entry === 'object' && !Array.isArray(entry)) {
      const commandRecord = entry as Record<string, unknown>;
      const command = typeof commandRecord.command === 'string' && commandRecord.command.trim().length > 0
        ? commandRecord.command
        : commandRecord.cmd;
      CommandValidator.validate(requireConsoleCommandString(command, `batch_console_commands.commands[${index}]`));
      continue;
    }

    throw new Error(`batch_console_commands.commands[${index}] must be a string or command object`);
  }
}

export async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: HandlerArgs,
  errorMessage: string = 'Automation bridge not available',
  options: {
    timeoutMs?: number;
    forwardTimeoutMsToUnreal?: boolean;
  } = {}
): Promise<unknown> {
  validateArgsSecurity(args);
  validateConsoleCommandPayload(toolName, args);

  const automationBridge = tools.automationBridge;
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }

  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
  }

  const timeoutMs = options.timeoutMs ?? (typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined);
  const cleanedArgs = { ...args };
  // forwardTimeoutMsToUnreal is an MRQ-specific escape hatch: start_render
  // sends its timeoutMs to Unreal so the native transport can apply per-job
  // deadlines. For all other actions, timeoutMs is a TS-side concern and
  // must be stripped before forwarding to avoid leaking it into the bridge.
  if (options.forwardTimeoutMsToUnreal) {
    const action = typeof cleanedArgs.action === 'string' ? cleanedArgs.action : undefined;
    if (toolName !== 'manage_sequence' || action !== 'start_render') {
      throw new Error(
        `forwardTimeoutMsToUnreal is only valid for manage_sequence/start_render (got ${toolName}${
          action ? `/${action}` : ''
        }).`,
      );
    }
  } else {
    delete cleanedArgs.timeoutMs;
  }

  return await automationBridge.sendAutomationRequest(toolName, cleanedArgs, timeoutMs ? { timeoutMs } : {});
}

export function createSubActionDispatcher(
  tools: ITools,
  args: HandlerArgs,
  options: {
    toolName: string;
    domainName: string;
    pathFields?: readonly string[];
    timeoutMs?: number;
    preparePayload?: (payload: Record<string, unknown>, subAction: string) => Record<string, unknown>;
  }
): SubActionDispatcher {
  const rawArgs: Record<string, unknown> = args;
  const argsRecord = options.pathFields && options.pathFields.length > 0
    ? normalizePathFields(rawArgs, options.pathFields)
    : { ...rawArgs };
  const timeoutMs = options.timeoutMs ?? (typeof argsRecord.timeoutMs === 'number' ? argsRecord.timeoutMs : getTimeoutMs());

  return {
    argsRecord,
    async sendRequest(subAction: string, extraPayload: Record<string, unknown> = {}): Promise<Record<string, unknown>> {
      const rawPayload = { ...argsRecord, ...extraPayload, subAction };
      const normalizedPayload = options.pathFields && options.pathFields.length > 0
        ? normalizePathFields(rawPayload, options.pathFields)
        : rawPayload;
      const payload = options.preparePayload ? options.preparePayload(normalizedPayload, subAction) : normalizedPayload;
      const result = await executeAutomationRequest(
        tools,
        options.toolName,
        payload,
        `Automation bridge not available for ${options.domainName} action: ${subAction}`,
        { timeoutMs }
      );
      return cleanObject(result) as Record<string, unknown>;
    }
  };
}

export async function executeBatchConsoleCommands(
  tools: ITools,
  commands: string[],
  options: { timeoutMs?: number } = {}
): Promise<{
  success: boolean;
  totalCommands: number;
  executedCount: number;
  failedCount: number;
}> {
  const validCommands = commands
    .map(command => command?.trim())
    .filter(command => command && command.length > 0);

  if (validCommands.length === 0) {
    return {
      success: true,
      totalCommands: 0,
      executedCount: 0,
      failedCount: 0
    };
  }

  const result = await executeAutomationRequest(
    tools,
    'batch_console_commands',
    { commands: validCommands },
    'Automation bridge not available for batch commands',
    options
  ) as {
    success?: boolean;
    totalCommands?: number;
    executedCount?: number;
    failedCount?: number;
    message?: string;
    error?: string;
  };

  const failedCount = result.failedCount ?? 0;
  if (result.success === false || failedCount > 0) {
    throw new Error(
      `Batch command execution failed: ${failedCount}/${validCommands.length} commands failed. ` +
      (result.message || result.error || 'Unknown error')
    );
  }

  return {
    success: true,
    totalCommands: result.totalCommands ?? validCommands.length,
    executedCount: result.executedCount ?? validCommands.length,
    failedCount: 0
  };
}
