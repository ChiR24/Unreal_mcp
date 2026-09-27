import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';
import { coerceNumber } from '../utils/validation/type-guards.js';

/** Backs ue://actors: every actor in the current level. */
export async function listActors(automationBridge: AutomationRequestBridge | undefined): Promise<Record<string, unknown>> {
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    return { success: false, error: 'Automation bridge is not available. Please ensure Unreal Engine is running with the MCP Automation Bridge plugin.' };
  }
  try {
    const resp = await automationBridge.sendAutomationRequest('control_actor', { action: 'list' }) as Record<string, unknown>;
    const result = resp?.result as Record<string, unknown> | undefined;
    const data = result?.data as Record<string, unknown> | unknown[] | undefined;
    const actors = [resp?.actors, result?.actors, data, (data as Record<string, unknown> | undefined)?.actors].find(Array.isArray) as unknown[] | undefined;
    if (resp && resp.success !== false && actors) {
      return { success: true, count: coerceNumber(resp.count) ?? coerceNumber(result?.count) ?? actors.length, actors };
    }
    return { success: false, error: 'Failed to retrieve actor list from automation bridge' };
  } catch (err) {
    return { success: false, error: `Failed to list actors: ${err instanceof Error ? err.message : String(err)}` };
  }
}
