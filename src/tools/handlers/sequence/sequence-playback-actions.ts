import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { setTimeout as delay } from 'node:timers/promises';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';
import { getErrorString, type SequenceActionResponse } from './sequence-handler-state.js';

export async function handleSequencePlaybackAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'play': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        startTime: args.startTime as number | undefined,
        loopMode: args.loopMode as 'once' | 'loop' | 'pingpong' | undefined,
        subAction: 'play'
      });
      return cleanObject(res);
    }
    case 'pause': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'pause'
      });
      return cleanObject(res);
    }
    case 'stop': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'stop'
      });
      return cleanObject(res);
    }
    case 'set_properties': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        frameRate: args.frameRate as number | undefined,
        lengthInFrames: args.lengthInFrames as number | undefined,
        playbackStart: args.playbackStart as number | undefined,
        playbackEnd: args.playbackEnd as number | undefined,
        subAction: 'set_properties'
      });
      return cleanObject(res);
    }
    case 'get_properties': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_properties'
      });
      return cleanObject(res);
    }
    case 'set_playback_speed': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const speed = Number(args.speed);
      if (!Number.isFinite(speed) || speed <= 0) {
        throw new Error('Invalid speed: must be a positive number');
      }

      let res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        speed,
        subAction: 'set_playback_speed'
      }) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      if ((!res || res.success === false) && errorCode === 'EDITOR_NOT_OPEN') {
        await executeAutomationRequest(tools, 'manage_sequence', {
          path,
          subAction: 'open'
        });

        await delay(1000);

        res = await executeAutomationRequest(tools, 'manage_sequence', {
          ...args,
          path,
          speed,
          subAction: 'set_playback_speed'
        }) as SequenceActionResponse;
      }

      return cleanObject(res);
    }
    case 'list': {
      const path = typeof args.path === 'string' ? args.path.trim() : undefined;
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'list'
      });
      return cleanObject(res);
    }
    default:
      return undefined;
  }
}
