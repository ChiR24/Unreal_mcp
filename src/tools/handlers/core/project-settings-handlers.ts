import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
export async function handleProjectSettingsTools(action: string, args: Record<string, unknown>, tools: any): Promise<Record<string, unknown>> {
  try {
    const payload = {
      ...args,
      subAction: action
    };

    const res = await executeAutomationRequest(tools, 'manage_project_settings', payload);
    return res as Record<string, unknown>;
  } catch (error) {
    return {
      success: false,
      message: `Failed to execute project settings action: ${action}`,
      error: error instanceof Error ? error.message : String(error)
    };
  }
}
