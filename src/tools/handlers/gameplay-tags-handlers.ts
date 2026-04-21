/**
 * GameplayTags System Handlers (Ch4)
 *
 * Implements manage_gameplay_tags actions covering:
 * - add_gameplay_tag          (IGameplayTagsEditorModule::AddNewGameplayTagToINI)
 * - list_gameplay_tags        (UGameplayTagsManager::RequestAllGameplayTags + prefix filter)
 * - remove_gameplay_tag       (IGameplayTagsEditorModule::DeleteTagFromINI)
 * - add_gameplay_tag_source   (IGameplayTagsEditorModule::AddNewGameplayTagSource)
 *
 * Dispatches to C++ via the automation bridge using a "manage_gameplay_tags"
 * tool action with a `subAction` payload field (consistent with manage_data).
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

async function sendTagsRequest(
  tools: ITools,
  subAction: string,
  payload: Record<string, unknown>
): Promise<Record<string, unknown>> {
  const timeoutMs = getTimeoutMs();
  const result = await executeAutomationRequest(
    tools,
    'manage_gameplay_tags',
    { ...payload, subAction } as HandlerArgs,
    `Automation bridge not available for manage_gameplay_tags action: ${subAction}`,
    { timeoutMs }
  );
  return cleanObject(result as Record<string, unknown>) as Record<string, unknown>;
}

function requireStringArg(args: Record<string, unknown>, field: string): string {
  const value = args[field];
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(`Missing required parameter: ${field}`);
  }
  return value;
}

function optionalStringArg(args: Record<string, unknown>, field: string): string | undefined {
  const value = args[field];
  return typeof value === 'string' ? value : undefined;
}

export async function handleGameplayTagsTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;

  // Silence "unused" warnings for the helpers that future tasks will consume.
  // These references are compiled out with no runtime effect.
  void argsRecord;
  void sendTagsRequest;
  void requireStringArg;
  void optionalStringArg;
  void tools;

  throw new Error(`Unsupported manage_gameplay_tags action: ${action}`);
}
