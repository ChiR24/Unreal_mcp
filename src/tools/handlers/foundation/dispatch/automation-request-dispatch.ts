import { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { CommandValidator } from '../../../../utils/commands/command-validator.js';
import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import { validateArgsSecurity } from '../arguments/handler-argument-validation.js';
import { getMcpRequestContext } from '../../../../automation/request-context.js';
import { getGatewayCorrelationId } from '../../../../automation/gateway-correlation-context.js';
import { getGatewayConsent } from '../../../../automation/gateway-consent-context.js';
import { getGatewayExpectedRevisions } from '../../../../automation/gateway-expected-revisions-context.js';
import type { ExpectedRevisions } from '../../../catalog/capabilities/semantic/execution-options.js';
import { resolveActionTimeoutMs } from './handler-timeout.js';
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
    mcpRequestId?: string;
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

  // No explicit budget: derive one from what this capability declares it costs.
  // Leaving it undefined would hand a metadata read and a full asset import the
  // same flat bridge default.
  const timeoutMs = options.timeoutMs
    ?? (typeof args.timeoutMs === 'number'
      ? args.timeoutMs
      : resolveActionTimeoutMs(toolName, typeof args.action === 'string' ? args.action : undefined));
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

  // Correlate this automation work to the active MCP request (if any) so the
  // bridge can cancel it on notifications/cancelled. An explicit option wins;
  // otherwise the async-local request context set by the tool registry is used.
  const mcpRequestId = options.mcpRequestId ?? getMcpRequestContext()?.requestId;

  // The single client-facing gateway correlation id (async-local) crosses here
  // as bounded request metadata so the outbound automation request, the plugin
  // queue and the receipt all join on the same id — never a handler param.
  const gatewayCorrelationId = getGatewayCorrelationId();

  // Validated per-call consent (async-local, set by the gateway) rides as an
  // automation_request envelope sibling for the plugin to re-validate; it is
  // never a handler param.
  const gatewayConsent = getGatewayConsent();

  const gatewayExpectedRevisions = getGatewayExpectedRevisions();

  const sendOptions: {
    timeoutMs?: number;
    mcpRequestId?: string;
    correlationId?: string;
    consent?: { capability: string; acknowledge: 'explicit' | 'elevated' };
    expectedRevisions?: ExpectedRevisions;
  } = {};
  if (timeoutMs !== undefined) sendOptions.timeoutMs = timeoutMs;
  if (mcpRequestId !== undefined) sendOptions.mcpRequestId = mcpRequestId;
  if (gatewayCorrelationId !== undefined) sendOptions.correlationId = gatewayCorrelationId;
  if (gatewayConsent !== undefined) sendOptions.consent = gatewayConsent;
  if (gatewayExpectedRevisions !== undefined) sendOptions.expectedRevisions = gatewayExpectedRevisions;

  return await automationBridge.sendAutomationRequest(toolName, cleanedArgs, sendOptions);
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
  const timeoutMs = options.timeoutMs
    ?? (typeof argsRecord.timeoutMs === 'number'
      ? argsRecord.timeoutMs
      : resolveActionTimeoutMs(options.toolName, typeof argsRecord.action === 'string' ? argsRecord.action : undefined));

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
