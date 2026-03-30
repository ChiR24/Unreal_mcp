import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleMrqTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown>> {
  const mrqAction = String(action || '').trim().toLowerCase();

  switch (mrqAction) {
    case 'get_queue':
    case 'get_job_config':
    case 'get_cvars':
    case 'get_output_settings':
    case 'get_render_status':
    case 'create_job':
    case 'delete_job':
    case 'render':
    case 'load_preset': {
      const res = await executeAutomationRequest(
        tools,
        'manage_mrq',
        { ...args, action: mrqAction },
        'MRQ automation bridge not available — ensure Movie Render Queue plugin is enabled'
      );
      return cleanObject(res) as Record<string, unknown>;
    }

    case 'set_cvars': {
      const payload: Record<string, unknown> = { ...args, action: mrqAction };
      if (args.cvars && typeof args.cvars === 'object' && !payload.set) {
        payload.set = args.cvars;
      }
      const res = await executeAutomationRequest(
        tools,
        'manage_mrq',
        payload,
        'MRQ automation bridge not available'
      );
      return cleanObject(res) as Record<string, unknown>;
    }

    case 'set_output_settings': {
      const res = await executeAutomationRequest(
        tools,
        'manage_mrq',
        { ...args, action: mrqAction },
        'MRQ automation bridge not available'
      );
      return cleanObject(res) as Record<string, unknown>;
    }

    default:
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown MRQ action: ${mrqAction}. Available: get_queue, get_job_config, get_cvars, set_cvars, get_output_settings, set_output_settings, create_job, delete_job, render, get_render_status, load_preset`
      };
  }
}
