import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';
import {
  getErrorString,
  getMessageString,
  type SequenceActionResponse
} from './sequence-handler-state.js';

export async function handleSequenceBindingAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'add_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const path = typeof args.path === 'string' ? args.path.trim() : '';

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        actorName,
        path: path || args.path,
        subAction: 'add_actor'
      }) as SequenceActionResponse;

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

      const msgLower = getMessageString(res).toLowerCase();
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
    default:
      return undefined;
  }
}
