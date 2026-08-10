import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';
import { validateRequiredFields } from '../foundation/arguments/batch-validation.js';
import type { SequenceActionResponse } from './sequence-handler-state.js';

export async function handleSequenceTrackAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'add_track': {
      const { path, trackType } = validateRequiredFields(args, ['path', 'trackType']);
      const trackName = typeof args.trackName === 'string' ? args.trackName : '';
      const actorName = typeof args.actorName === 'string' ? args.actorName : undefined;

      if (actorName) {
        const bindingsRes = await executeAutomationRequest(tools, 'manage_sequence', {
          path,
          subAction: 'get_bindings'
        }) as SequenceActionResponse;
        if (bindingsRes && bindingsRes.success) {
          const bindings = bindingsRes.bindings || [];
          const isBound = bindings.some((b) => b.name === actorName);
          if (!isBound) {
            return cleanObject({
              success: false,
              error: 'BINDING_NOT_FOUND',
              message: `Actor '${actorName}' is not bound to this sequence. Please call 'add_actor' first.`,
              action: 'add_track',
              path,
              actorName
            });
          }
        }
      }

      const payload = {
        ...args,
        path,
        trackType,
        trackName,
        actorName,
        subAction: 'add_track'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload);
      return cleanObject(res);
    }
    case 'add_section': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = { ...args, path, subAction: 'add_section' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'remove_track': {
      const { path, trackName } = validateRequiredFields(args, ['path', 'trackName']);
      const payload = { ...args, path, trackName, subAction: 'remove_track' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_muted': {
      const { path, trackName } = validateRequiredFields(args, ['path', 'trackName']);
      const payload = { ...args, path, trackName, subAction: 'set_track_muted' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_solo': {
      const { path, trackName } = validateRequiredFields(args, ['path', 'trackName']);
      const payload = { ...args, path, trackName, subAction: 'set_track_solo' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_locked': {
      const { path, trackName } = validateRequiredFields(args, ['path', 'trackName']);
      const payload = { ...args, path, trackName, subAction: 'set_track_locked' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'list_tracks': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'list_tracks'
      });
      return cleanObject(res);
    }
    case 'set_work_range': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const start = Number(args.start);
      const end = Number(args.end);
      if (!Number.isFinite(start)) throw new Error('Invalid start: must be a number');
      if (!Number.isFinite(end)) throw new Error('Invalid end: must be a number');

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        start,
        end,
        subAction: 'set_work_range'
      });
      return cleanObject(res);
    }
    case 'set_tick_resolution': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const resolution = args.resolution;
      if (resolution === undefined || resolution === null) {
        throw new Error('Missing required parameter: resolution');
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        resolution,
        subAction: 'set_tick_resolution'
      });
      return cleanObject(res);
    }
    case 'set_view_range': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const start = args.start !== undefined ? Number(args.start) : undefined;
      const end = args.end !== undefined ? Number(args.end) : undefined;

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        start,
        end,
        subAction: 'set_view_range'
      });
      return cleanObject(res);
    }
    default:
      return undefined;
  }
}
