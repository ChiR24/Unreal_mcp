import type { StandardActionResponse } from './types/tools/tool-interfaces.js';
import { setTimeout as delay } from 'node:timers/promises';
import { CONSOLE_COMMAND_TIMEOUT_MS } from './constants.js';
import { CommandValidator } from './utils/commands/command-validator.js';
import { UnrealCommandQueueStoppedError } from './utils/commands/unreal-command-queue.js';
import type {
  BatchConsoleCommand,
  BatchConsoleOptions,
  BatchConsoleResult,
  BridgeConsoleResponse,
  ConsoleCommandContext,
  PartialBatchConsoleResult
} from './unreal-bridge-types.js';

function isNonEmptyString(value: string | undefined): value is string {
  return value !== undefined && value.length > 0;
}

export async function executeConsoleCommand(
  context: ConsoleCommandContext,
  command: string
): Promise<StandardActionResponse> {
  CommandValidator.validate(command);
  const cmdTrimmed = command.trim();
  if (cmdTrimmed.length === 0) {
    return { success: true, message: 'Empty command ignored' };
  }

  if (CommandValidator.isLikelyInvalid(cmdTrimmed)) {
    context.log.warn(`Command appears invalid: ${cmdTrimmed}`);
  }

  const priority = CommandValidator.getPriority(cmdTrimmed);
  const bridge = context.getAutomationBridge();
  const automationAvailable = process.env.MOCK_UNREAL_CONNECTION === 'true' || Boolean(
    bridge && typeof bridge.sendAutomationRequest === 'function'
  );
  if (!automationAvailable) {
    throw new Error('Automation bridge not connected');
  }

  const executeCommand = async (): Promise<StandardActionResponse> => {
    if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
      context.log.info(`[MOCK] Executing console command: ${cmdTrimmed}`);
      return { success: true, message: `Mock execution of '${cmdTrimmed}' successful`, transport: 'mock_bridge' };
    }

    const activeBridge = context.getAutomationBridge();
    if (!activeBridge || !activeBridge.isConnected()) {
      throw new Error('Automation bridge not connected');
    }

    const pluginResp = await activeBridge.sendAutomationRequest<BridgeConsoleResponse>(
      'console_command',
      { command: cmdTrimmed },
      { timeoutMs: CONSOLE_COMMAND_TIMEOUT_MS }
    );

    if (pluginResp.success) {
      return { success: true, ...pluginResp, transport: 'automation_bridge' };
    }

    throw new Error(pluginResp.message || pluginResp.error || 'Plugin execution failed');
  };

  try {
    return await context.runThrottled(executeCommand, priority);
  } catch (error) {
    if (!(error instanceof UnrealCommandQueueStoppedError)) {
      context.log.error(`Console command failed: ${cmdTrimmed}`, error);
    }
    throw error;
  }
}

export async function executeConsoleCommands(
  context: ConsoleCommandContext,
  commands: Iterable<string | BatchConsoleCommand>,
  options: BatchConsoleOptions = {}
): Promise<unknown[]> {
  const { continueOnError = false, delayMs = 0 } = options;
  const results: unknown[] = [];

  for (const rawCommand of commands) {
    const descriptor = typeof rawCommand === 'string' ? { command: rawCommand } : rawCommand;
    const command = descriptor.command?.trim();
    if (!command) {
      continue;
    }
    try {
      results.push(await executeConsoleCommand(context, command));
    } catch (error) {
      if (!continueOnError) {
        throw error;
      }
      context.log.warn(`Console batch command failed: ${command}`, error);
      results.push(error);
    }

    if (delayMs > 0) {
      await delay(delayMs);
    }
  }

  return results;
}

export async function executeBatchConsoleCommands(
  context: ConsoleCommandContext,
  commands: readonly string[],
  options: { readonly timeoutMs?: number } = {}
): Promise<BatchConsoleResult> {
  const validCommands = commands
    .map((command): string | undefined => typeof command === 'string' ? command.trim() : undefined)
    .filter(isNonEmptyString);

  if (validCommands.length === 0) {
    return {
      success: true,
      totalCommands: 0,
      executedCount: 0,
      failedCount: 0,
      results: []
    };
  }

  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    context.log.info(`[MOCK] Batch executing ${validCommands.length} console commands`);
    return {
      success: true,
      totalCommands: validCommands.length,
      executedCount: validCommands.length,
      failedCount: 0,
      results: validCommands.map(command => ({ command, success: true }))
    };
  }

  const bridge = context.getAutomationBridge();
  if (!bridge || !bridge.isConnected()) {
    throw new Error('Automation bridge not connected');
  }

  for (const command of validCommands) {
    CommandValidator.validate(command);
  }

  const timeoutMs = options.timeoutMs ?? CONSOLE_COMMAND_TIMEOUT_MS * Math.max(1, Math.ceil(validCommands.length / 10));
  const result = await bridge.sendAutomationRequest<PartialBatchConsoleResult>(
    'batch_console_commands',
    { commands: validCommands },
    { timeoutMs }
  );

  return {
    success: result.success !== false,
    totalCommands: result.totalCommands ?? validCommands.length,
    executedCount: result.executedCount ?? 0,
    failedCount: result.failedCount ?? 0,
    results: result.results ?? validCommands.map(command => ({ command, success: true }))
  };
}
