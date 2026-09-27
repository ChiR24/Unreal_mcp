import { ITools } from '../../../../types/tools/tool-interfaces.js';
import { CommandValidator } from '../../../../utils/commands/command-validator.js';
import { validateArgsSecurity } from '../arguments/handler-argument-validation.js';
import { getMcpRequestContext } from '../../../../automation/request-context.js';
import type { ExpectedRevisions } from '../../../catalog/capabilities/semantic/execution-options.js';
import { resolveActionTimeoutMs } from './handler-timeout.js';

/**
 * Gateway controls for one execute: never handler params. They ride the
 * automation_request envelope for the plugin to re-validate.
 */
export interface GatewayControls {
  readonly timeoutMs?: number;
  readonly correlationId?: string;
  readonly consent?: { capability: string; acknowledge: 'explicit' | 'elevated' };
  readonly expectedRevisions?: ExpectedRevisions;
}

// The actions that run args.command verbatim. Checked here before the request
// leaves the process; the plugin re-checks every one.
const CONSOLE_ACTIONS = new Set(['console_command', 'execute_command']);

function firstNonEmpty(args: Record<string, unknown>, keys: readonly string[]): string {
  for (const key of keys) {
    const value = args[key];
    if (typeof value === 'string' && value.trim() !== '') return value.trim();
  }
  return '';
}

// The console command the plugin will build from caller strings for the actions
// that compose one (UiHandlersSystemExtras.cpp). The plugin only re-checks the
// native rule subset, so the TypeScript-only rules must see the composed string.
function composedConsoleCommand(action: string | undefined, args: Record<string, unknown>): string | undefined {
  if (action === 'set_cvar') {
    const name = firstNonEmpty(args, ['name', 'cvar', 'key', 'command']);
    if (name === '') return undefined;
    const raw = args.value;
    const value = typeof raw === 'boolean' ? (raw ? '1' : '0')
      : typeof raw === 'number' || typeof raw === 'string' ? String(raw) : '';
    return value === '' ? name : `${name} ${value}`;
  }
  if (action === 'set_resolution' || action === 'set_fullscreen') {
    const resolution = firstNonEmpty(args, ['resolution']);
    return resolution === '' ? undefined : `r.SetRes ${resolution}`;
  }
  return undefined;
}

export async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: Record<string, unknown>,
  controls: GatewayControls = {}
): Promise<unknown> {
  validateArgsSecurity(args);
  const action = typeof args.action === 'string' ? args.action : undefined;
  if (CONSOLE_ACTIONS.has(action ?? toolName)) {
    CommandValidator.validate(typeof args.command === 'string' ? args.command : '');
  }
  const composed = composedConsoleCommand(action, args);
  if (composed !== undefined) CommandValidator.validate(composed);

  const automationBridge = tools.automationBridge;
  if (!automationBridge) {
    throw new Error('Automation bridge not available');
  }
  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
  }

  // The client's own deadline (options.timeoutMs) wins; otherwise the budget
  // comes from the cost class the capability declares.
  const timeoutMs = controls.timeoutMs ?? resolveActionTimeoutMs(toolName, action);

  const sendOptions: {
    timeoutMs?: number;
    mcpRequestId?: string;
    correlationId?: string;
    consent?: { capability: string; acknowledge: 'explicit' | 'elevated' };
    expectedRevisions?: ExpectedRevisions;
  } = { timeoutMs };
  const mcpRequestId = getMcpRequestContext()?.requestId;
  const { correlationId, consent, expectedRevisions } = controls;
  if (mcpRequestId !== undefined) sendOptions.mcpRequestId = mcpRequestId;
  if (correlationId !== undefined) sendOptions.correlationId = correlationId;
  if (consent !== undefined) sendOptions.consent = consent;
  if (expectedRevisions !== undefined) sendOptions.expectedRevisions = expectedRevisions;

  return await automationBridge.sendAutomationRequest(toolName, args, sendOptions);
}
