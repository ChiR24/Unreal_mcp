import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';
import { validateRequiredFields } from '../foundation/arguments/batch-validation.js';
import {
  getErrorString,
  getMessageString,
  type SequenceActionResponse
} from './sequence-handler-state.js';

export async function handleSequenceCoreAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'create': {
      const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
      const basePath = typeof args.path === 'string' ? args.path.trim().replace(/\/$/, '') : '/Game/Sequences';

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        name,
        path: basePath,
        subAction: 'create'
      }) as SequenceActionResponse;

      let sequencePath: string | undefined;
      if (res && res.result && typeof res.result.sequencePath === 'string') {
        sequencePath = res.result.sequencePath;
      } else if (typeof args.path === 'string' && args.path.trim().length > 0) {
        const p = args.path.trim().replace(/\/$/, '');
        sequencePath = `${p}/${name}`;
      }
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: false,
          error: 'FACTORY_NOT_AVAILABLE',
          message: res.message || 'Sequence creation failed: factory not available',
          action: 'create',
          name,
          path,
          sequencePath,
          handled: true
        });
      }

      return cleanObject(res);
    }
    case 'open': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'open'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_camera': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        spawnable: args.spawnable !== false,
        subAction: 'add_camera'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const path = typeof args.path === 'string' ? args.path.trim() : '';
      const payload = {
        ...args,
        actorName,
        path: path || args.path,
        subAction: 'add_actor'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      if (res && res.success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence_add_actor requires a sequence path') || msgLower.includes('sequence not found');
        if (isInvalidSequence) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_actor',
            path,
            actorName
          });
        }
      }

      const results = res && res.result && Array.isArray(res.result.results)
        ? res.result.results
        : undefined;
      if (results && results.length) {
        const failed = results.find((item) => item && item.success === false && typeof item.error === 'string');
        if (failed) {
          const errText = String(failed.error).toLowerCase();
          if (errText.includes('actor not found')) {
            return cleanObject({
              success: false,
              error: 'NOT_FOUND',
              message: failed.error,
              action: 'add_actor',
              path: path || undefined,
              actorName
            });
          }
        }
      }

      return cleanObject(res);
    }
    case 'add_actors': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      if (actorNames.length === 0) {
        throw new Error('Missing required parameter: actorNames (must be non-empty array)');
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        actorNames,
        path,
        subAction: 'add_actors'
      }) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (actorNames.length === 0 && res && res.success === false && errorCode === 'INVALID_ARGUMENT') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: res.message || 'Invalid argument: actorNames required',
          action: 'add_actors',
          actorNames
        });
      }
      if (res && res.success === false && msgLower.includes('actor not found')) {
        return cleanObject({
          success: false,
          error: 'NOT_FOUND',
          message: res.message || 'Actor not found',
          action: 'add_actors',
          actorNames
        });
      }
      return cleanObject(res);
    }
    case 'remove_actors': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      if (actorNames.length === 0) {
        throw new Error('Missing required parameter: actorNames (must be non-empty array)');
      }
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        actorNames,
        path,
        subAction: 'remove_actors'
      });
      return cleanObject(res);
    }
    case 'get_bindings': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_bindings'
      });
      return cleanObject(res);
    }
    case 'add_keyframe': {
      const { path, actorName } = validateRequiredFields(args, ['path', 'actorName']);
      const property = typeof args.property === 'string' ? args.property : 'Transform';
      const frame = typeof args.frame === 'number' ? args.frame : Number(args.frame);
      if (!Number.isFinite(frame)) {
        throw new Error('Missing or invalid required parameter: frame (must be a number)');
      }

      const payload: Record<string, unknown> = {
        ...args,
        path,
        actorName,
        property,
        frame,
        subAction: 'add_keyframe'
      };

      if (property === 'Location') {
        payload.property = 'Transform';
        payload.value = { location: args.value };
      } else if (property === 'Rotation') {
        payload.property = 'Transform';
        payload.value = { rotation: args.value };
      } else if (property === 'Scale') {
        payload.property = 'Transform';
        payload.value = { scale: args.value };
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      if (errorCode === 'INVALID_ARGUMENT' || msgLower.includes('frame number is required')) {
        return cleanObject(res);
      }

      if (res && res.success === false) {
        const isBindingIssue = errorCode === 'BINDING_NOT_FOUND' || msgLower.includes('binding not found');
        const isUnsupported = errorCode === 'UNSUPPORTED_PROPERTY' || msgLower.includes('unsupported property') || msgLower.includes('invalid_sequence_type');
        const isInvalidSeq = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');

        if (path && isInvalidSeq) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_keyframe',
            path,
            actorName,
            property,
            frame
          });
        }

        if (path && (isBindingIssue || isUnsupported)) {
          return cleanObject(res);
        }
      }

      return cleanObject(res);
    }
    case 'add_spawnable_from_class': {
      const { className, path } = validateRequiredFields(args, ['className', 'path']);
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        className,
        path,
        subAction: 'add_spawnable_from_class'
      });
      return cleanObject(res);
    }
    default:
      return undefined;
  }
}
