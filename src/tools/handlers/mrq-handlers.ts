import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireAction } from './common-handlers.js';
import { CommandValidator } from '../../utils/command-validator.js';

/**
 * Normalize asset path fields to canonical /Game/... form.
 */
function normalizePathFields(args: Record<string, unknown>, fields: string[]): Record<string, unknown> {
  const result = { ...args };
  for (const field of fields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      if (!normalized.startsWith('/')) {
        normalized = '/Game/' + normalized;
      }
      result[field] = normalized;
    }
  }
  return result;
}

/**
 * Validate an array of console commands using the existing safety filter.
 */
function validateCommands(commands: unknown): string[] | undefined {
  if (!Array.isArray(commands)) return undefined;
  const validated: string[] = [];
  for (const cmd of commands) {
    if (typeof cmd === 'string') {
      CommandValidator.validate(cmd); // throws on dangerous commands
      validated.push(cmd);
    }
  }
  return validated;
}

export async function handleMrqTools(
  _action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown>> {
  const mrqAction = requireAction(args);

  switch (mrqAction) {
    case 'get_queue':
    case 'get_job_config':
    case 'get_cvars':
    case 'get_output_settings':
    case 'get_render_status':
    case 'delete_job':
    case 'render': {
      const res = await executeAutomationRequest(
        tools,
        'manage_mrq',
        { ...args, action: mrqAction },
        'MRQ automation bridge not available — ensure Movie Render Queue plugin is enabled'
      );
      return cleanObject(res) as Record<string, unknown>;
    }

    case 'create_job':
    case 'load_preset': {
      // Normalize asset paths to canonical /Game/... form
      const normalized = normalizePathFields(args, ['map', 'sequence', 'presetPath']);
      const res = await executeAutomationRequest(
        tools,
        'manage_mrq',
        { ...normalized, action: mrqAction },
        'MRQ automation bridge not available — ensure Movie Render Queue plugin is enabled'
      );
      return cleanObject(res) as Record<string, unknown>;
    }

    case 'set_cvars': {
      const payload: Record<string, unknown> = { ...args, action: mrqAction };
      if (args.cvars && typeof args.cvars === 'object' && !payload.set) {
        payload.set = args.cvars;
      }
      // Validate start/end console commands through safety filter
      if (payload.startCommands) {
        payload.startCommands = validateCommands(payload.startCommands);
      }
      if (payload.endCommands) {
        payload.endCommands = validateCommands(payload.endCommands);
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
